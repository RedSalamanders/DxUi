#include "DxUi.MarkdownLabels.h"
#include <DxUi/RichTextExport.h>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace DxUi::RichText
{
namespace
{
void Append(std::wstring& output, std::wstring_view text)
{
    if (text.size() > ExportContent::MaximumUnits - output.size())
        throw std::length_error("Rich text export capacity exceeded");
    output.append(text);
}
void Escape(std::wstring& output, std::wstring_view text)
{
    for (const auto unit : text)
        switch (unit)
        {
            case L'&': Append(output, L"&amp;"); break;
            case L'<': Append(output, L"&lt;"); break;
            case L'>': Append(output, L"&gt;"); break;
            case L'"': Append(output, L"&quot;"); break;
            case L'\'': Append(output, L"&#39;"); break;
            default:
            {
                const wchar_t safe = (unit < L' ' && unit != L'\n' && unit != L'\r' && unit != L'\t') || unit == 0x7fu ? L'\ufffd' : unit;
                Append(output, std::wstring_view(&safe, 1u));
                break;
            }
        }
}
void EscapeInline(std::wstring& output, std::wstring_view text)
{
    std::size_t first = 0u;
    for (std::size_t index = 0u; index < text.size(); ++index)
    {
        if (text[index] != L'\n' && text[index] != L'\r')
            continue;
        Escape(output, text.substr(first, index - first));
        Append(output, L"<br />\n");
        if (text[index] == L'\r' && index + 1u < text.size() && text[index + 1u] == L'\n')
            ++index;
        first = index + 1u;
    }
    Escape(output, text.substr(first));
}
bool Prefix(std::wstring_view text, std::wstring_view prefix) noexcept
{
    if (text.size() < prefix.size())
        return false;
    for (std::size_t index = 0u; index < prefix.size(); ++index)
    {
        auto unit = text[index];
        if (unit >= L'A' && unit <= L'Z')
            unit = static_cast<wchar_t>(unit + (L'a' - L'A'));
        if (unit != prefix[index])
            return false;
    }
    return true;
}
bool SafeLink(std::wstring_view value) noexcept
{
    return ! value.empty() && std::ranges::none_of(value, [](wchar_t unit) { return unit <= L' ' || unit == 0x7fu; }) &&
           (value.front() == L'#' || Prefix(value, L"https://") || Prefix(value, L"http://") || Prefix(value, L"mailto:"));
}
template <typename Element> bool IsUnresolvedWiki(const Element& element) noexcept
{
    if constexpr (requires { element.wiki; })
        return element.wiki;
    else
        return false;
}
template <typename Role> bool IsAbbreviation(Role role) noexcept
{
    if constexpr (requires { Role::Abbreviation; })
        return role == Role::Abbreviation;
    else
        return false;
}
template <typename Role> std::wstring_view ExtensionTag(Role role) noexcept
{
    if constexpr (requires { Role::Disclosure; })
        if (role == Role::Disclosure)
            return L"p";
    if constexpr (requires { Role::Callout; })
        if (role == Role::Callout)
            return L"aside";
    if constexpr (requires {
                      Role::DefinitionList;
                      Role::DefinitionTerm;
                      Role::DefinitionDescription;
                  })
    {
        if (role == Role::DefinitionList)
            return L"dl";
        if (role == Role::DefinitionTerm)
            return L"dt";
        if (role == Role::DefinitionDescription)
            return L"dd";
    }
    return {};
}
template <typename Role> bool ExtensionLeaf(Role role) noexcept
{
    if constexpr (requires { Role::Disclosure; })
        if (role == Role::Disclosure)
            return true;
    if constexpr (requires {
                      Role::DefinitionTerm;
                      Role::DefinitionDescription;
                  })
        if (role == Role::DefinitionTerm || role == Role::DefinitionDescription)
            return true;
    return false;
}
class Serializer
{
    const SemanticDocument& document;
    std::wstring_view display;
    MarkdownRange selected;
    std::vector<const SemanticElement*> links, objects, abbreviations;
    std::vector<const SemanticElement*> blockedFragments;
    std::vector<std::wstring_view> headingIds;
    std::vector<const SemanticHeading*> headingsBySlug;
    std::vector<std::size_t> markerEnds;
    std::wstring_view Alternative(const SemanticElement& element) const
    {
        if (element.role == SemanticRole::Rule)
            return L"---";
        return element.role == SemanticRole::Diagram ? document.Syntax()->Slice(element.source) : std::wstring_view(element.name);
    }
    const SemanticElement* Find(const std::vector<const SemanticElement*>& entries, std::size_t offset) const noexcept
    {
        auto found = std::upper_bound(entries.begin(), entries.end(), offset, [](std::size_t at, const auto* entry) { return at < entry->display.first; });
        if (found == entries.begin())
            return nullptr;
        const auto* entry = *--found;
        return offset < entry->display.last ? entry : nullptr;
    }
    static std::wstring_view Tag(SemanticRole role, unsigned heading = 0u) noexcept
    {
        if (role == SemanticRole::Quote)
            return L"blockquote";
        if (role == SemanticRole::ListItem)
            return L"li";
        if (role == SemanticRole::Table)
            return L"table";
        if (role == SemanticRole::Row)
            return L"tr";
        if (role == SemanticRole::Cell)
            return L"td";
        if (role == SemanticRole::Footnote)
            return L"section";
        if (role == SemanticRole::Paragraph)
            return L"p";
        if (role == SemanticRole::Heading)
        {
            constexpr std::array<std::wstring_view, 6u> headings{L"h1", L"h2", L"h3", L"h4", L"h5", L"h6"};
            return headings[std::clamp(heading, 1u, 6u) - 1u];
        }
        return ExtensionTag(role);
    }
    void Open(std::wstring& output, std::wstring_view tag) const
    {
        Append(output, L"<");
        Append(output, tag);
        Append(output, L">");
    }
    void Close(std::wstring& output, std::wstring_view tag) const
    {
        Append(output, L"</");
        Append(output, tag);
        Append(output, L">");
    }
    void Inline(std::wstring& output, MarkdownRange range, bool insidePre = false) const
    {
        if (range.first > range.last || range.last > display.size())
            throw std::logic_error("Invalid export display range");
        std::vector<std::size_t> boundaries(0u);
        boundaries.push_back(range.first);
        boundaries.push_back(range.last);
        const auto add = [&](MarkdownRange span)
        {
            if (span.first < range.last && span.last > range.first)
            {
                boundaries.push_back(std::max(span.first, range.first));
                boundaries.push_back(std::min(span.last, range.last));
            }
        };
        auto runs = document.Runs();
        auto run  = std::lower_bound(runs.begin(), runs.end(), range.first, [](const auto& item, std::size_t at) { return item.start + item.length <= at; });
        for (auto item = run; item != runs.end() && item->start < range.last; ++item)
            add({item->start, item->start + item->length});
        for (const auto& entries : {&links, &objects, &abbreviations})
        {
            auto item =
                std::lower_bound(entries->begin(), entries->end(), range.first, [](const auto* entry, std::size_t at) { return entry->display.last <= at; });
            for (; item != entries->end() && (*item)->display.first < range.last; ++item)
                add((*item)->display);
        }
        std::sort(boundaries.begin(), boundaries.end());
        boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
        for (std::size_t index = 1u; index < boundaries.size(); ++index)
        {
            const auto first = boundaries[index - 1u], last = boundaries[index];
            while (run != runs.end() && run->start + run->length <= first)
                ++run;
            const auto flags  = (run != runs.end() && run->start <= first ? run->flags : Regular) & (insidePre ? ~Code : ~0u);
            const auto* link  = Find(links, first);
            const bool anchor = link && ! IsUnresolvedWiki(*link) && SafeLink(link->destination) && ! std::ranges::binary_search(blockedFragments, link);
            if (anchor)
            {
                Append(output, L"<a href=\"");
                Escape(output, link->destination);
                Append(output, L"\" rel=\"noreferrer noopener\"");
                if (! link->title.empty())
                {
                    Append(output, L" title=\"");
                    Escape(output, link->title);
                    Append(output, L"\"");
                }
                Append(output, L">");
            }
            const auto* abbreviation = Find(abbreviations, first);
            if (abbreviation)
            {
                Append(output, L"<abbr title=\"");
                Escape(output, abbreviation->title);
                Append(output, L"\">");
            }
            const std::pair<unsigned, std::wstring_view> styles[] = {{Bold, L"strong"},
                                                                     {Italic, L"em"},
                                                                     {Strike, L"del"},
                                                                     {Underline, L"u"},
                                                                     {Marked, L"mark"},
                                                                     {Superscript, L"sup"},
                                                                     {Subscript, L"sub"},
                                                                     {Code, L"code"}};
            for (const auto& style : styles)
                if (flags & style.first)
                    Open(output, style.second);
            const auto* object = Find(objects, first);
            if (object && first == object->display.first && last == object->display.last)
            {
                if (! insidePre)
                    Open(output, L"code");
                Escape(output, Alternative(*object));
                if (! insidePre)
                    Close(output, L"code");
            }
            else if (insidePre)
                Escape(output, display.substr(first, last - first));
            else
                EscapeInline(output, display.substr(first, last - first));
            for (auto style = std::rbegin(styles); style != std::rend(styles); ++style)
                if (flags & style->first)
                    Close(output, style->second);
            if (abbreviation)
                Close(output, L"abbr");
            if (anchor)
                Close(output, L"a");
        }
    }

public:
    Serializer(const Serializer&)            = delete;
    Serializer& operator=(const Serializer&) = delete;
    Serializer(Serializer&&)                 = delete;
    Serializer& operator=(Serializer&&)      = delete;
    explicit Serializer(const SemanticDocument& value, MarkdownRange range)
        : document(value),
          display(value.Map().Display()),
          selected(range),
          links(0u),
          objects(0u),
          abbreviations(0u),
          blockedFragments(0u),
          headingIds(value.Elements().size()),
          headingsBySlug(0u),
          markerEnds(value.Elements().size(), 0u)
    {
        if (range.first > range.last || range.last > display.size() || ! IsEditBoundary(display, range.first) || ! IsEditBoundary(display, range.last))
            throw std::invalid_argument("Invalid export display range");
        for (const auto& heading : document.Headings())
        {
            if (heading.element < headingIds.size())
            {
                headingIds[heading.element] = heading.slug;
                headingsBySlug.push_back(&heading);
            }
        }
        std::ranges::sort(headingsBySlug, {}, [](const auto* heading) { return std::wstring_view(heading->slug); });
        for (const auto& paragraph : document.Paragraphs())
            if (paragraph.element < markerEnds.size())
                markerEnds[paragraph.element] = std::max(markerEnds[paragraph.element], paragraph.markerEnd);
        for (std::size_t index = 0u; index < document.Elements().size(); ++index)
        {
            const auto& element = document.Elements()[index];
            if (document.IsSourceEditingElement(index))
                continue;
            if (element.role == SemanticRole::Link && element.display.first < element.display.last)
            {
                links.push_back(&element);
                if (element.destination.starts_with(L'#'))
                {
                    bool included = element.destination == L"#" && selected.first == 0u;
                    if (! included && element.destination != L"#")
                    {
                        std::wstring wanted(0u, L' ');
                        if (Detail::DecodeHeadingFragment(element.destination, wanted))
                        {
                            const auto found = std::ranges::lower_bound(
                                headingsBySlug, std::wstring_view(wanted), {}, [](const auto* heading) { return std::wstring_view(heading->slug); });
                            if (found != headingsBySlug.end() && (*found)->slug == wanted)
                            {
                                const auto headingDisplay = document.Elements()[(*found)->element].display;
                                included                  = headingDisplay.first < selected.last && headingDisplay.last > selected.first;
                            }
                        }
                    }
                    if (! included)
                        blockedFragments.push_back(&element);
                }
            }
            if ((element.role == SemanticRole::Math || element.role == SemanticRole::Diagram || element.role == SemanticRole::Rule) &&
                element.display.last == element.display.first + 1u && element.display.last <= display.size() && display[element.display.first] == L'\ufffc')
                objects.push_back(&element);
            if (IsAbbreviation(element.role) && ! element.title.empty() && element.display.first < element.display.last)
                abbreviations.push_back(&element);
        }
        const auto order = [](const auto* left, const auto* right)
        { return left->display.first < right->display.first || (left->display.first == right->display.first && left->display.last > right->display.last); };
        std::sort(links.begin(), links.end(), order);
        std::sort(objects.begin(), objects.end(), order);
        std::sort(abbreviations.begin(), abbreviations.end(), order);
        std::ranges::sort(blockedFragments);
    }
    void Text(std::wstring& result) const
    {
        std::size_t first = selected.first;
        for (const auto* object : objects)
        {
            if (object->display.last <= selected.first || object->display.first >= selected.last)
                continue;
            Append(result, display.substr(first, object->display.first - first));
            Append(result, Alternative(*object));
            first = object->display.last;
        }
        Append(result, display.substr(first, selected.last - first));
    }
    void Html(std::wstring& result) const
    {
        struct Visit
        {
            std::size_t element;
            bool close;
        };
        std::vector<Visit> pending{{0u, false}};
        const auto elements = document.Elements();
        if (selected.first == selected.last)
            return;
        while (! pending.empty())
        {
            const auto visit = pending.back();
            pending.pop_back();
            const auto& element = elements[visit.element];
            auto tag            = Tag(element.role, element.heading);
            if (element.role == SemanticRole::List)
            {
                const auto nodes = document.Syntax()->Nodes();
                tag = element.syntaxNode < nodes.size() && (nodes[element.syntaxNode].listMarker == L'.' || nodes[element.syntaxNode].listMarker == L')')
                          ? L"ol"
                          : L"ul";
            }
            if (element.role == SemanticRole::Cell && element.tableHeader)
                tag = L"th";
            const bool literal = element.role == SemanticRole::Code || (element.role == SemanticRole::Html && ! element.nativeHtml) ||
                                 element.role == SemanticRole::Metadata || element.role == SemanticRole::Diagram;
            if (element.role == SemanticRole::Html && element.nativeHtml)
                tag = element.heading ? Tag(SemanticRole::Heading, element.heading) : L"p";
            if (literal)
                tag = L"pre";
            if (visit.close)
            {
                if (! tag.empty())
                {
                    Close(result, tag);
                    Append(result, L"\n");
                }
                continue;
            }
            if (element.nextSibling != MarkdownNoNode)
                pending.push_back({element.nextSibling, false});
            if (element.display.last <= selected.first || element.display.first >= selected.last)
                continue;
            if (element.role == SemanticRole::Rule)
            {
                Append(result, L"<hr />\n");
                continue;
            }
            if (tag == L"ol" && element.syntaxNode < document.Syntax()->Nodes().size())
            {
                auto start = document.Syntax()->Nodes()[element.syntaxNode].listStart;
                for (auto child = element.firstChild; child != MarkdownNoNode; child = elements[child].nextSibling)
                {
                    if (elements[child].display.last > selected.first)
                        break;
                    if (elements[child].role == SemanticRole::ListItem)
                        ++start;
                }
                Append(result, L"<ol start=\"");
                std::array<wchar_t, 10u> digits{};
                auto at = digits.size();
                do
                {
                    digits[--at] = static_cast<wchar_t>(L'0' + start % 10u);
                    start /= 10u;
                } while (start);
                Append(result, std::wstring_view(digits.data() + at, digits.size() - at));
                Append(result, L"\">");
            }
            else if ((element.role == SemanticRole::Heading || (element.role == SemanticRole::Html && element.heading)) && visit.element < headingIds.size() &&
                     ! headingIds[visit.element].empty())
            {
                Append(result, L"<");
                Append(result, tag);
                Append(result, L" id=\"");
                Escape(result, headingIds[visit.element]);
                Append(result, L"\">");
            }
            else if (! tag.empty())
                Open(result, tag);
            const bool leaf = literal || element.role == SemanticRole::Paragraph || element.role == SemanticRole::Heading ||
                              element.role == SemanticRole::Cell || element.role == SemanticRole::Html || ExtensionLeaf(element.role);
            if (leaf)
            {
                auto range = MarkdownRange{std::max(element.display.first, selected.first), std::min(element.display.last, selected.last)};
                if (markerEnds[visit.element] > range.first && markerEnds[visit.element] <= range.last)
                    range.first = markerEnds[visit.element];
                if (literal)
                    Open(result, L"code");
                Inline(result, range, literal);
                if (literal)
                    Close(result, L"code");
                if (! tag.empty())
                {
                    Close(result, tag);
                    Append(result, L"\n");
                }
            }
            else
            {
                if (! tag.empty())
                    pending.push_back({visit.element, true});
                if (element.firstChild != MarkdownNoNode)
                    pending.push_back({element.firstChild, false});
                else if (tag.empty() && element.display.first < element.display.last)
                    Inline(result, {std::max(element.display.first, selected.first), std::min(element.display.last, selected.last)});
            }
        }
    }
};
} // namespace
ExportContent ExportSemanticDocument(const SemanticDocument& document)
{
    return ExportSemanticDocument(document, {0u, document.Map().Display().size()});
}
ExportContent ExportSemanticDocument(const SemanticDocument& document, MarkdownRange displayRange)
{
    const Serializer serializer(document, displayRange);
    std::wstring text(0u, L' '), html(0u, L' ');
    serializer.Text(text);
    serializer.Html(html);
    return ExportContent(std::move(text), std::move(html));
}
ExportContent ExportMarkdownSyntax(std::shared_ptr<const MarkdownSyntax> syntax)
{
    if (! syntax)
        throw std::invalid_argument("Export requires a Markdown syntax publication");
    SemanticBuilder builder;
    builder.Begin(std::move(syntax));
    while (builder.HasPendingPreparation())
        static_cast<void>(builder.Advance());
    const auto document = builder.Published();
    if (! document)
        throw std::logic_error("Markdown export did not publish a semantic document");
    return ExportSemanticDocument(*document);
}
} // namespace DxUi::RichText
