#include "DxUi.MarkdownMetadata.h"
#include <DxUi/Failure.h>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#pragma warning(push)
#pragma warning(disable : 4464)
#include "../../ThirdParty/libfyaml/0.9.4/dxui_allocator.h"
#include "../../ThirdParty/libfyaml/0.9.4/dxui_parser.h"
#pragma warning(pop)

extern "C" __declspec(noreturn) void dxui_fy_fatal_allocation(std::size_t bytes)
{
    DxUi::Failure::FailAllocation(bytes);
}
extern "C" __declspec(noreturn) void dxui_fy_fatal_contract(void)
{
    DxUi::Failure::FailContract(E_UNEXPECTED);
}

namespace DxUi::Failure::Detail
{
void SetYamlNativeAllocationFailureForTest(std::size_t successfulAllocations) noexcept
{
    dxui_fy_fail_native_after_for_test(successfulAllocations);
}
} // namespace DxUi::Failure::Detail

namespace DxUi::RichText::Detail
{
namespace
{

struct YamlNode
{
    explicit YamlNode(bool isMap, bool isSeq) : map(isMap), sequence(isSeq), scalar(0u, '\0'), tag(0u, '\0'), children(0u)
    {
    }
    bool map{}, sequence{}, plain{};
    std::string scalar, tag;
    std::vector<std::size_t> children;
};
struct YamlAnchor
{
    YamlAnchor(std::string_view value, std::size_t target) : name(value), node(target)
    {
    }
    std::string name;
    std::size_t node{};
};
struct YamlGraph
{
    YamlGraph() : nodes(0u), stack(0u), anchors(0u), pendingAnchor(0u, '\0'), pendingTag(0u, '\0')
    {
    }
    std::vector<std::unique_ptr<YamlNode>> nodes;
    std::vector<std::size_t> stack;
    std::vector<std::unique_ptr<YamlAnchor>> anchors;
    std::string pendingAnchor, pendingTag;
    std::size_t rootCount{};

    MetadataIssue Add(bool map, bool sequence, bool plain, std::string_view scalar, std::size_t alias = (std::numeric_limits<std::size_t>::max)())
    {
        if (nodes.size() >= 4096u || stack.size() >= 128u)
            return MetadataIssue::Capacity;
        std::size_t index = alias;
        if (alias == (std::numeric_limits<std::size_t>::max)())
        {
            auto node   = std::make_unique<YamlNode>(map, sequence);
            node->plain = plain;
            node->scalar.assign(scalar);
            node->tag.assign(pendingTag);
            index = nodes.size();
            nodes.push_back(std::move(node));
            if (! pendingAnchor.empty())
                anchors.push_back(std::make_unique<YamlAnchor>(pendingAnchor, index));
        }
        if (stack.empty())
            ++rootCount;
        else
            nodes[stack.back()]->children.push_back(index);
        pendingAnchor.clear();
        pendingTag.clear();
        if (map || sequence)
            stack.push_back(index);
        return MetadataIssue::None;
    }
};

struct FyBudgetScope
{
    dxui_fy_budget budget{};
    explicit FyBudgetScope(std::size_t limit)
    {
        dxui_fy_budget_begin(&budget, limit);
    }
    ~FyBudgetScope()
    {
        dxui_fy_budget_end(&budget);
    }
};
struct FyParserScope
{
    fy_parser* parser{};
    void Close()
    {
        if (parser)
            dxui_fy_guard_destroy(parser);
        parser = nullptr;
    }
    ~FyParserScope()
    {
        Close();
    }
};
struct FyEventScope
{
    fy_parser* parser{};
    fy_event* event{};
    ~FyEventScope()
    {
        if (event)
            dxui_fy_guard_event_free(parser, event);
    }
};
bool FyText(fy_token* token, std::string_view& result)
{
    if (! token)
    {
        result = {};
        return true;
    }
    std::size_t size{};
    const char* text = dxui_fy_guard_token_text(token, &size);
    if (! text)
        return false;
    result = std::string_view(text, size);
    return true;
}
MetadataIssue MakeYamlGraph(std::wstring_view text, YamlGraph& graph, std::size_t backendBudget)
{
    std::string source;
    if (! MetadataUtf8(text, source))
        return MetadataIssue::Invalid;

    // Every vendor entry goes through the C-only status fence. A refused entry
    // poisons the parser, and its arena then frees storage without inspecting it.
    FyBudgetScope budget{backendBudget};
    FyParserScope owner{dxui_fy_guard_create()};
    if (! owner.parser)
    {
        if (budget.budget.exceeded)
            return MetadataIssue::Capacity;
        DxUi::Failure::FailUnexpected();
    }
    if (dxui_fy_guard_set_string(owner.parser, source.data(), source.size()) != 0)
    {
        if (budget.budget.exceeded)
            return MetadataIssue::Capacity;
        DxUi::Failure::FailUnexpected();
    }
    const auto parseEvents = [&]() -> MetadataIssue
    {
        std::size_t documents{}, eventWords{}, arenaBytes{}, eventDepth{};
        MetadataIssue graphIssue = MetadataIssue::None;
        for (;;)
        {
            FyEventScope current{owner.parser, dxui_fy_guard_parse(owner.parser)};
            if (budget.budget.exceeded)
                return MetadataIssue::Capacity;
            if (! current.event)
                return MetadataIssue::Invalid;
            const auto& event = *current.event;
            if (++eventWords > 256u * 1024u)
                return MetadataIssue::Capacity;
            if (event.type == FYET_STREAM_END)
                break;
            if (event.type == FYET_DOCUMENT_START)
            {
                if (++documents > 1u)
                    return MetadataIssue::Invalid;
                void* previous{};
                std::size_t directives{};
                while (const auto* tag = dxui_fy_guard_tag_next(event.document_start.document_state, &previous))
                {
                    const bool isDefault = dxui_fy_guard_tag_is_default(event.document_start.document_state, tag) != 0;
                    if (budget.budget.exceeded)
                        return MetadataIssue::Capacity;
                    if (! isDefault && ++directives > 128u)
                        return MetadataIssue::Capacity;
                }
                if (budget.budget.exceeded)
                    return MetadataIssue::Capacity;
                continue;
            }
            // Enforce depth even after an unresolved alias stopped graph creation.
            if (event.type == FYET_MAPPING_START || event.type == FYET_SEQUENCE_START)
            {
                if (++eventDepth > 128u)
                    return MetadataIssue::Capacity;
            }
            else if (event.type == FYET_MAPPING_END || event.type == FYET_SEQUENCE_END)
            {
                if (! eventDepth)
                    return MetadataIssue::Invalid;
                --eventDepth;
            }
            // Keep scanning after graph refusal: a later syntax error has precedence.
            if (graphIssue != MetadataIssue::None)
                continue;
            if (event.type == FYET_ALIAS)
            {
                eventWords += 2u;
                std::string_view name;
                const bool haveName = FyText(event.alias.anchor, name);
                if (budget.budget.exceeded)
                    return MetadataIssue::Capacity;
                if (! haveName)
                    return MetadataIssue::Invalid;
                auto found = graph.anchors.rend();
                for (auto it = graph.anchors.rbegin(); it != graph.anchors.rend(); ++it)
                    if ((*it)->name == name)
                    {
                        found = it;
                        break;
                    }
                if (found == graph.anchors.rend())
                {
                    graphIssue = MetadataIssue::UnresolvedAlias;
                    continue;
                }
                graphIssue = graph.Add(false, false, false, {}, (*found)->node);
                continue;
            }
            if (event.type == FYET_MAPPING_END || event.type == FYET_SEQUENCE_END)
            {
                if (graph.stack.empty())
                {
                    graphIssue = MetadataIssue::Invalid;
                    continue;
                }
                graph.stack.pop_back();
                continue;
            }
            const bool scalar   = event.type == FYET_SCALAR;
            const bool map      = event.type == FYET_MAPPING_START;
            const bool sequence = event.type == FYET_SEQUENCE_START;
            if (! scalar && ! map && ! sequence)
                continue;
            auto* anchor = scalar ? event.scalar.anchor : map ? event.mapping_start.anchor : event.sequence_start.anchor;
            auto* tag    = scalar ? event.scalar.tag : map ? event.mapping_start.tag : event.sequence_start.tag;
            std::string_view value;
            const bool haveValue = ! scalar || FyText(event.scalar.value, value);
            if (budget.budget.exceeded)
                return MetadataIssue::Capacity;
            if (! haveValue)
                return MetadataIssue::Invalid;
            eventWords += 2u + (anchor ? 3u : 0u) + (tag ? 3u : 0u);
            if (eventWords > 256u * 1024u)
                return MetadataIssue::Capacity;
            const bool plain    = scalar && dxui_fy_guard_scalar_style(event.scalar.value) == FYSS_PLAIN;
            const auto addition = scalar && ! plain ? value.size() : 0u;
            if (addition > 512u * 1024u - arenaBytes)
                return MetadataIssue::Capacity;
            arenaBytes += addition;
            std::string_view anchorText;
            const bool haveAnchor = FyText(anchor, anchorText);
            if (budget.budget.exceeded)
                return MetadataIssue::Capacity;
            if (! haveAnchor)
                return MetadataIssue::Invalid;
            graph.pendingAnchor.assign(anchorText);
            graph.pendingTag.clear();
            if (tag)
            {
                std::string_view tagText;
                const bool haveTag = FyText(tag, tagText); // Full resolved and decoded tag URI.
                if (budget.budget.exceeded)
                    return MetadataIssue::Capacity;
                if (! haveTag)
                    return MetadataIssue::Invalid;
                graph.pendingTag.assign(tagText);
            }
            graphIssue = graph.Add(map, sequence, plain, value);
        }
        if (graphIssue != MetadataIssue::None)
            return graphIssue;
        return graph.stack.empty() && graph.rootCount <= 1u ? MetadataIssue::None : MetadataIssue::Invalid;
    };
    const auto issue = parseEvents();
    // Cleanup can allocate inside the backend. Complete it before choosing the
    // final status, so a refused cleanup cannot accidentally publish success.
    owner.Close();
    return budget.budget.exceeded ? MetadataIssue::Capacity : issue;
}

bool YamlInteger(std::string_view input, std::string& out, MetadataIssue& issue)
{
    if (input.empty())
        return false;
    std::size_t pos        = 0u;
    const bool negative    = input[pos] == '-';
    const bool signedValue = input[pos] == '-' || input[pos] == '+';
    if (signedValue)
        ++pos;
    if (pos == input.size())
        return false;
    unsigned base = 10u;
    if (input.substr(pos, 2u) == "0x")
    {
        base = 16u;
        pos += 2u;
    }
    else if (input.substr(pos, 2u) == "0o")
    {
        base = 8u;
        pos += 2u;
    }
    if (pos == input.size() || (signedValue && base != 10u))
        return false;
    for (std::size_t cursor = pos; cursor < input.size(); ++cursor)
    {
        const char c         = input[cursor];
        const unsigned digit = c >= '0' && c <= '9'   ? static_cast<unsigned>(c - '0')
                               : c >= 'a' && c <= 'f' ? static_cast<unsigned>(c - 'a') + 10u
                               : c >= 'A' && c <= 'F' ? static_cast<unsigned>(c - 'A') + 10u
                                                      : 99u;
        if (digit >= base)
            return false;
    }
    if (input.size() - pos > 256u)
    {
        issue = MetadataIssue::Capacity;
        return false;
    }
    std::string decimal(1u, '0');
    for (; pos < input.size(); ++pos)
    {
        const char c         = input[pos];
        const unsigned digit = c >= '0' && c <= '9'   ? static_cast<unsigned>(c - '0')
                               : c >= 'a' && c <= 'f' ? static_cast<unsigned>(c - 'a') + 10u
                               : c >= 'A' && c <= 'F' ? static_cast<unsigned>(c - 'A') + 10u
                                                      : 99u;
        if (digit >= base)
            return false;
        unsigned carry = digit;
        for (auto it = decimal.rbegin(); it != decimal.rend(); ++it)
        {
            const auto value = static_cast<unsigned>(*it - '0') * base + carry;
            *it              = static_cast<char>('0' + value % 10u);
            carry            = value / 10u;
        }
        while (carry)
        {
            decimal.insert(decimal.begin(), static_cast<char>('0' + carry % 10u));
            carry /= 10u;
        }
    }
    out = negative && decimal != "0" ? "-" + decimal : decimal;
    return true;
}

bool YamlFloat(std::string_view input, std::string& out, bool explicitFloat, MetadataIssue& issue)
{
    if (input.empty())
        return false;
    std::size_t pos        = 0u;
    const bool negative    = input[pos] == '-';
    const bool signedValue = input[pos] == '-' || input[pos] == '+';
    if (signedValue)
        ++pos;
    const auto body = input.substr(pos);
    if (body == ".inf" || body == ".Inf" || body == ".INF")
    {
        out = negative ? "-inf" : "+inf";
        return true;
    }
    if (! signedValue && (body == ".nan" || body == ".NaN" || body == ".NAN"))
    {
        out = "nan";
        return true;
    }
    std::string digits(0u, '\0');
    std::size_t before = 0u, after = 0u;
    while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9')
    {
        digits.push_back(input[pos++]);
        ++before;
    }
    bool dot = false;
    if (pos < input.size() && input[pos] == '.')
    {
        dot = true;
        ++pos;
        while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9')
        {
            digits.push_back(input[pos++]);
            ++after;
        }
    }
    if (! before && ! after)
        return false;
    bool exponent = false;
    int exp       = 0;
    if (pos < input.size() && (input[pos] == 'e' || input[pos] == 'E'))
    {
        exponent = true;
        ++pos;
        const auto start = pos;
        if (pos < input.size() && (input[pos] == '-' || input[pos] == '+'))
            ++pos;
        const auto digitsStart = pos;
        while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9')
            ++pos;
        if (pos == digitsStart)
            return false;
        auto expText     = input.substr(start, pos - start);
        bool expNegative = false;
        if (expText.front() == '-' || expText.front() == '+')
        {
            expNegative = expText.front() == '-';
            expText.remove_prefix(1u);
        }
        if (expText.size() > 6u)
        {
            issue = MetadataIssue::Capacity;
            return false;
        }
        for (const auto c : expText)
            exp = exp * 10 + (c - '0');
        if (expNegative)
            exp = -exp;
    }
    if ((! dot && ! exponent && ! explicitFloat) || pos != input.size())
        return false;
    if (digits.size() > 256u)
    {
        issue = MetadataIssue::Capacity;
        return false;
    }
    const auto first = digits.find_first_not_of('0');
    if (first == std::string::npos)
    {
        out = "0";
        return true;
    }
    digits.erase(0u, first);
    exp -= static_cast<int>(after);
    while (digits.back() == '0')
    {
        digits.pop_back();
        ++exp;
    }
    out.clear();
    out.push_back(negative ? '-' : '+');
    out.append(digits);
    out.push_back('e');
    out.append(std::to_string(exp));
    return true;
}

std::string YamlScalarKey(const YamlNode& node, MetadataIssue& issue)
{
    const auto value       = std::string_view(node.scalar);
    const auto tag         = std::string_view(node.tag);
    const bool implicit    = tag.empty() || tag == "?";
    const bool forceString = (! node.plain && implicit) || tag == "!" || tag == "!!str" || tag == "tag:yaml.org,2002:str";
    const bool integer     = tag == "!!int" || tag == "tag:yaml.org,2002:int";
    const bool floating    = tag == "!!float" || tag == "tag:yaml.org,2002:float";
    const bool boolean     = tag == "!!bool" || tag == "tag:yaml.org,2002:bool";
    const bool null        = tag == "!!null" || tag == "tag:yaml.org,2002:null";
    if (tag == "!!map" || tag == "!!seq" || tag == "tag:yaml.org,2002:map" || tag == "tag:yaml.org,2002:seq")
    {
        issue = MetadataIssue::Invalid;
        return {};
    }
    if (forceString)
        return "s" + node.scalar;
    if (implicit || null)
        if (value.empty() || value == "~" || value == "null" || value == "Null" || value == "NULL")
            return "n";
    if (implicit || boolean)
    {
        if (value == "true" || value == "True" || value == "TRUE")
            return "b1";
        if (value == "false" || value == "False" || value == "FALSE")
            return "b0";
    }
    std::string normalized(0u, '\0');
    if ((implicit || integer) && YamlInteger(value, normalized, issue))
        return "i" + normalized;
    if (issue != MetadataIssue::None)
        return {};
    if ((implicit || floating) && YamlFloat(value, normalized, floating, issue))
        return "f" + normalized;
    if (issue != MetadataIssue::None)
        return {};
    if (integer || floating || boolean || null)
    {
        issue = MetadataIssue::Invalid;
        return {};
    }
    if (! implicit)
    {
        std::string result("t");
        result.append(std::to_string(node.tag.size()));
        result.push_back(':');
        result.append(node.tag);
        result.append(node.scalar);
        return std::string(std::string_view(result));
    }
    return "s" + node.scalar;
}

struct CanonicalBudget
{
    std::size_t visits{}, bytes{};
    MetadataIssue issue{MetadataIssue::None};
};
void AppendYamlPart(std::string& target, std::string_view value, CanonicalBudget& budget)
{
    if (budget.issue != MetadataIssue::None)
        return;
    if (value.size() > 8u * 1024u * 1024u - budget.bytes)
    {
        budget.issue = MetadataIssue::Capacity;
        return;
    }
    budget.bytes += value.size();
    if (target.size() + value.size() > 1024u * 1024u)
    {
        budget.issue = MetadataIssue::Capacity;
        return;
    }
    target += std::to_string(value.size());
    target.push_back(':');
    target.append(value);
}

std::string CanonicalYamlNode(const YamlGraph& graph, std::size_t index, std::vector<unsigned char>& active, std::size_t depth, CanonicalBudget& budget)
{
    if (budget.issue != MetadataIssue::None)
        return {};
    if (++budget.visits > 100000u || depth >= 128u || active[index])
    {
        budget.issue = MetadataIssue::Capacity;
        return {};
    }
    const auto& node = *graph.nodes[index];
    if (! node.map && ! node.sequence)
        return YamlScalarKey(node, budget.issue);
    active[index] = 1u;
    std::string result(node.map ? "m" : "q");
    const bool ordinaryTag = node.tag.empty() || node.tag == "!" || node.tag == "?" ||
                             (node.map && (node.tag == "!!map" || node.tag == "tag:yaml.org,2002:map")) ||
                             (node.sequence && (node.tag == "!!seq" || node.tag == "tag:yaml.org,2002:seq"));
    AppendYamlPart(result, ordinaryTag ? std::string_view{} : std::string_view(node.tag), budget);
    if (node.map)
    {
        if (node.children.size() % 2u)
        {
            budget.issue = MetadataIssue::Invalid;
            return {};
        }
        std::vector<std::unique_ptr<std::string>> pairs(0u);
        for (std::size_t i = 0u; i < node.children.size(); i += 2u)
        {
            auto pair = std::make_unique<std::string>(0u, '\0');
            AppendYamlPart(*pair, CanonicalYamlNode(graph, node.children[i], active, depth + 1u, budget), budget);
            AppendYamlPart(*pair, CanonicalYamlNode(graph, node.children[i + 1u], active, depth + 1u, budget), budget);
            if (budget.issue != MetadataIssue::None)
            {
                active[index] = 0u;
                return {};
            }
            pairs.push_back(std::move(pair));
        }
        std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) { return *a < *b; });
        for (const auto& pair : pairs)
            AppendYamlPart(result, *pair, budget);
    }
    else
        for (const auto child : node.children)
            AppendYamlPart(result, CanonicalYamlNode(graph, child, active, depth + 1u, budget), budget);
    active[index] = 0u;
    return std::string(std::string_view(result));
}

MetadataIssue CheckYamlKeys(const YamlGraph& graph)
{
    std::vector<unsigned char> active(graph.nodes.size(), 0u);
    CanonicalBudget budget;
    for (const auto& node : graph.nodes)
    {
        if (! node->map && ! node->sequence)
        {
            if (! node->tag.empty() && node->tag != "!" && node->tag != "?")
            {
                static_cast<void>(YamlScalarKey(*node, budget.issue));
                if (budget.issue != MetadataIssue::None)
                    return budget.issue;
            }
            continue;
        }
        const auto& tag = node->tag;
        if ((node->map && (tag == "!!seq" || tag == "tag:yaml.org,2002:seq")) || (node->sequence && (tag == "!!map" || tag == "tag:yaml.org,2002:map")) ||
            tag == "!!str" || tag == "!!int" || tag == "!!float" || tag == "!!bool" || tag == "!!null" || tag == "tag:yaml.org,2002:str" ||
            tag == "tag:yaml.org,2002:int" || tag == "tag:yaml.org,2002:float" || tag == "tag:yaml.org,2002:bool" || tag == "tag:yaml.org,2002:null")
            return MetadataIssue::Invalid;
        if (! node->map)
            continue;
        if (node->children.size() % 2u)
            return MetadataIssue::Invalid;
        std::vector<std::unique_ptr<std::string>> keys(0u);
        for (std::size_t i = 0u; i < node->children.size(); i += 2u)
        {
            const auto canonical = CanonicalYamlNode(graph, node->children[i], active, 0u, budget);
            if (budget.issue != MetadataIssue::None)
                return budget.issue;
            auto key = std::make_unique<std::string>(canonical);
            if (key->size() > 8u * 1024u * 1024u - budget.bytes)
                return MetadataIssue::Capacity;
            budget.bytes += key->size();
            keys.push_back(std::move(key));
        }
        std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return *a < *b; });
        for (std::size_t i = 1u; i < keys.size(); ++i)
            if (*keys[i - 1u] == *keys[i])
                return MetadataIssue::DuplicateKey;
    }
    return MetadataIssue::None;
}
} // namespace

MetadataIssue ValidateYamlMetadata(std::wstring_view text, std::size_t backendBudget)
{
    const DxUi::Failure::Scope scope;
    // YAML 1.2.2 c-printable excludes raw control characters even inside
    // comments or quoted scalars. Markdown admission already checks pairs.
    for (const auto unit : text)
    {
        const auto scalar = static_cast<unsigned>(unit);
        if (scalar == 0x09u || scalar == 0x0au || scalar == 0x0du || (scalar >= 0x20u && scalar <= 0x7eu) || scalar == 0x85u ||
            (scalar >= 0xa0u && scalar <= 0xfffdu))
            continue;
        return MetadataIssue::Invalid;
    }
    YamlGraph graph;
    const auto graphIssue = MakeYamlGraph(text, graph, backendBudget);
    return graphIssue == MetadataIssue::None ? CheckYamlKeys(graph) : graphIssue;
}
} // namespace DxUi::RichText::Detail
