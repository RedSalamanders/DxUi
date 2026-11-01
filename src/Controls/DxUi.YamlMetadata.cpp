#include "DxUi.MarkdownMetadata.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

// The upstream integer-event amalgamation omits its event handler. This private
// copy includes the exact upstream handler and isolates C4 symbols from consumers.
#define RYML_SINGLE_HDR_DEFINE_NOW
#define RYML_WITH_TAB_TOKENS
#define RYML_MAX_TAG_DIRECTIVES 128
#define c4 dxui_private_c4
#pragma warning(push)
#pragma warning(disable : 4464)
#include "../../ThirdParty/rapidyaml/0.16.0/rapidyaml.hpp"
#pragma warning(pop)
#undef c4

namespace DxUi::RichText::Detail
{
namespace
{
namespace yaml   = dxui_private_c4::yml;
namespace events = dxui_private_c4::yml::extra::ievt;
struct YamlSyntaxError
{
};
struct YamlCapacityError
{
};
struct Budget
{
    std::size_t bytes{};
    static constexpr std::size_t MaximumBytes = 4u * 1024u * 1024u;
};
void* AllocateYaml(std::size_t length, void*, void* context)
{
    auto& budget = *static_cast<Budget*>(context);
    if (length > Budget::MaximumBytes - budget.bytes)
        throw YamlCapacityError{};
    auto* const result = ::operator new(length);
    budget.bytes += length;
    return result;
}
void FreeYaml(void* allocation, std::size_t length, void* context)
{
    auto& budget = *static_cast<Budget*>(context);
    budget.bytes -= length;
    ::operator delete(allocation);
}
[[noreturn]] void YamlBasicError(dxui_private_c4::csubstr, const yaml::ErrorDataBasic&, void*)
{
    throw YamlSyntaxError{};
}
[[noreturn]] void YamlParseError(dxui_private_c4::csubstr message, const yaml::ErrorDataParse&, void*)
{
    if (std::string_view(message.str, message.len) == "too many %TAG directives")
        throw YamlCapacityError{};
    throw YamlSyntaxError{};
}
[[noreturn]] void YamlVisitError(dxui_private_c4::csubstr, const yaml::ErrorDataVisit&, void*)
{
    throw YamlSyntaxError{};
}
struct EventStream
{
    EventStream() : source(0u, '\0'), arena(0u, '\0'), words(0u)
    {
    }
    std::string source;
    std::string arena;
    std::vector<events::evt_bits> words;
};
MetadataIssue ParseYamlEvents(std::wstring_view text, EventStream& out)
{
    if (! MetadataUtf8(text, out.source))
        return MetadataIssue::Invalid;
    // The handler counts required output even when a bounded buffer fills.
    // Exhaustion is a capacity result, not a malformed-YAML diagnosis.
    constexpr std::size_t MaximumWords = 256u * 1024u;
    constexpr std::size_t MaximumArena = 512u * 1024u;
    out.words.resize((std::min)(MaximumWords, out.source.size() * 8u + 64u));
    out.arena.resize((std::min)(MaximumArena, out.source.size() * 3u + 64u));
    Budget budget;
    yaml::Callbacks callbacks;
    callbacks.m_user_data   = &budget;
    callbacks.m_allocate    = AllocateYaml;
    callbacks.m_free        = FreeYaml;
    callbacks.m_error_basic = YamlBasicError;
    callbacks.m_error_parse = YamlParseError;
    callbacks.m_error_visit = YamlVisitError;
    try
    {
        yaml::extra::EventHandlerInts handler(callbacks);
        yaml::ParserOptions options;
        options.resolve_tags_all(true);
        yaml::ParseEngine<yaml::extra::EventHandlerInts> parser(&handler, options);
        const dxui_private_c4::substr input(out.source.data(), out.source.size());
        handler.reset(
            input, dxui_private_c4::substr(out.arena.data(), out.arena.size()), out.words.data(), static_cast<yaml::extra::evt_size>(out.words.size()));
        parser.parse_in_place_ev(dxui_private_c4::csubstr("front-matter"), input);
        if (! handler.fits_buffers())
            return MetadataIssue::Capacity;
        out.words.resize(static_cast<std::size_t>(handler.required_size_events()));
        return MetadataIssue::None;
    }
    catch (const YamlSyntaxError&)
    {
        return MetadataIssue::Invalid;
    }
    catch (const YamlCapacityError&)
    {
        return MetadataIssue::Capacity;
    }
}
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

MetadataIssue MakeYamlGraph(const EventStream& stream, YamlGraph& graph)
{
    std::vector<std::unique_ptr<std::string>> directiveHandles(0u);
    bool versionSeen = false;
    for (std::size_t index = 0u; index < stream.words.size();)
    {
        const auto bits = stream.words[index++];
        std::string_view value;
        if (bits & events::WSTR)
        {
            if (index + 1u >= stream.words.size())
                return MetadataIssue::Invalid;
            const auto offset = static_cast<std::size_t>(stream.words[index++]);
            const auto length = static_cast<std::size_t>(stream.words[index++]);
            const auto& bytes = bits & events::AREN ? stream.arena : stream.source;
            if (offset > bytes.size() || length > bytes.size() - offset)
                return MetadataIssue::Invalid;
            value = std::string_view(bytes).substr(offset, length);
        }
        if (bits & events::YAML)
        {
            if (versionSeen)
                return MetadataIssue::Invalid;
            versionSeen = true;
        }
        else if (bits & events::TAGH)
        {
            for (const auto& prior : directiveHandles)
                if (*prior == value)
                    return MetadataIssue::Invalid;
            directiveHandles.push_back(std::make_unique<std::string>(value));
        }
        else if (bits & events::ANCH)
            graph.pendingAnchor.assign(value);
        else if (bits & events::TAG_)
        {
            if (value.size() >= 2u && value.front() == '<' && value.back() == '>')
                value = value.substr(1u, value.size() - 2u);
            graph.pendingTag.assign(value);
        }
        else if (bits & events::ALIA)
        {
            auto found = graph.anchors.rend();
            for (auto it = graph.anchors.rbegin(); it != graph.anchors.rend(); ++it)
                if ((*it)->name == value)
                {
                    found = it;
                    break;
                }
            if (found == graph.anchors.rend())
                return MetadataIssue::UnresolvedAlias;
            const auto issue = graph.Add(false, false, false, {}, (*found)->node);
            if (issue != MetadataIssue::None)
                return issue;
        }
        else if (bits & events::SCLR)
        {
            const auto issue = graph.Add(false, false, (bits & events::PLAI) != 0, value);
            if (issue != MetadataIssue::None)
                return issue;
        }
        else if ((bits & events::BEG_) && (bits & (events::MAP_ | events::SEQ_)))
        {
            const auto issue = graph.Add((bits & events::MAP_) != 0, (bits & events::SEQ_) != 0, false, {});
            if (issue != MetadataIssue::None)
                return issue;
        }
        else if ((bits & events::END_) && (bits & (events::MAP_ | events::SEQ_)))
        {
            if (graph.stack.empty())
                return MetadataIssue::Invalid;
            graph.stack.pop_back();
        }
        else if ((bits & events::BDOC) == events::BDOC)
        {
            // An envelope owns one YAML document and anchors never cross it.
            if (graph.rootCount)
                return MetadataIssue::Invalid;
        }
    }
    return graph.stack.empty() && graph.rootCount <= 1u ? MetadataIssue::None : MetadataIssue::Invalid;
}

bool YamlInteger(std::string_view input, std::string& out)
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
        throw YamlCapacityError{};
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

bool YamlFloat(std::string_view input, std::string& out, bool explicitFloat)
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
            throw YamlCapacityError{};
        for (const auto c : expText)
            exp = exp * 10 + (c - '0');
        if (expNegative)
            exp = -exp;
    }
    if ((! dot && ! exponent && ! explicitFloat) || pos != input.size())
        return false;
    if (digits.size() > 256u)
        throw YamlCapacityError{};
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

std::string YamlScalarKey(const YamlNode& node)
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
        throw YamlSyntaxError{};
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
    if ((implicit || integer) && YamlInteger(value, normalized))
        return "i" + normalized;
    if ((implicit || floating) && YamlFloat(value, normalized, floating))
        return "f" + normalized;
    if (integer || floating || boolean || null)
        throw YamlSyntaxError{};
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
};
void AppendYamlPart(std::string& target, std::string_view value, CanonicalBudget& budget)
{
    if (value.size() > 8u * 1024u * 1024u - budget.bytes)
        throw YamlCapacityError{};
    budget.bytes += value.size();
    if (target.size() + value.size() > 1024u * 1024u)
        throw YamlCapacityError{};
    target += std::to_string(value.size());
    target.push_back(':');
    target.append(value);
}

std::string CanonicalYamlNode(const YamlGraph& graph, std::size_t index, std::vector<unsigned char>& active, std::size_t depth, CanonicalBudget& budget)
{
    if (++budget.visits > 100000u || depth >= 128u || active[index])
        throw YamlCapacityError{};
    const auto& node = *graph.nodes[index];
    if (! node.map && ! node.sequence)
        return YamlScalarKey(node);
    active[index] = 1u;
    std::string result(node.map ? "m" : "q");
    const bool ordinaryTag = node.tag.empty() || node.tag == "!" || node.tag == "?" ||
                             (node.map && (node.tag == "!!map" || node.tag == "tag:yaml.org,2002:map")) ||
                             (node.sequence && (node.tag == "!!seq" || node.tag == "tag:yaml.org,2002:seq"));
    AppendYamlPart(result, ordinaryTag ? std::string_view{} : std::string_view(node.tag), budget);
    if (node.map)
    {
        if (node.children.size() % 2u)
            throw YamlSyntaxError{};
        std::vector<std::unique_ptr<std::string>> pairs(0u);
        for (std::size_t i = 0u; i < node.children.size(); i += 2u)
        {
            auto pair = std::make_unique<std::string>(0u, '\0');
            AppendYamlPart(*pair, CanonicalYamlNode(graph, node.children[i], active, depth + 1u, budget), budget);
            AppendYamlPart(*pair, CanonicalYamlNode(graph, node.children[i + 1u], active, depth + 1u, budget), budget);
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
                static_cast<void>(YamlScalarKey(*node));
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
            auto key = std::make_unique<std::string>(canonical);
            if (key->size() > 8u * 1024u * 1024u - budget.bytes)
                throw YamlCapacityError{};
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

MetadataIssue ValidateYamlMetadata(std::wstring_view text)
{
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
    EventStream events;
    const auto issue = ParseYamlEvents(text, events);
    if (issue != MetadataIssue::None)
        return issue;
    try
    {
        YamlGraph graph;
        const auto graphIssue = MakeYamlGraph(events, graph);
        return graphIssue == MetadataIssue::None ? CheckYamlKeys(graph) : graphIssue;
    }
    catch (const YamlSyntaxError&)
    {
        return MetadataIssue::Invalid;
    }
    catch (const YamlCapacityError&)
    {
        return MetadataIssue::Capacity;
    }
}
} // namespace DxUi::RichText::Detail
