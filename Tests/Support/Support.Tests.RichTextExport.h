#pragma once
#include "Support.Tests.ClipboardFixture.h"
#include "Support.Tests.RichSemanticDocument.h"
#include <DxUi/Configuration.h>
#include <DxUi/RichTextExport.h>
#include <stdexcept>

#if DXUI_ENABLE_DIAGNOSTICS
namespace DxUi::RichText::Detail
{
extern thread_local std::size_t testRichExportUnitLimit;
}
#endif

namespace RichTextExportTests
{
template <typename Check> void OptionalSemanticExtensions(Check&& check)
{
    using namespace DxUi::RichText;
    MarkdownParser parser;
    parser.Begin(L"HTML\n\n*[HTML]: Hyper Text\n\nTerm\n: **description**\n",
                 1u,
                 MarkdownProfile::RichMarkdown,
                 MarkdownExtensions::RichAll | MarkdownExtensions::DefinitionLists | MarkdownExtensions::Abbreviations);
    while (parser.HasPendingPreparation())
        parser.Advance();
    const auto syntax = parser.Published();
    SemanticBuilder builder;
    builder.Begin(syntax);
    const auto document = RichSemanticDocumentTests::Complete(builder, *syntax);
    const auto content  = ExportSemanticDocument(*document);
    check(content.HtmlFragment().find(L"<abbr title=\"Hyper Text\">HTML</abbr>") != std::wstring_view::npos &&
              content.HtmlFragment().find(L"<dl>") != std::wstring_view::npos && content.HtmlFragment().find(L"<dt>") != std::wstring_view::npos &&
              content.HtmlFragment().find(L"<dd>") != std::wstring_view::npos &&
              content.HtmlFragment().find(L"<strong>description</strong>") != std::wstring_view::npos,
          "optional abbreviation and definition roles retain their inert HTML semantics");
}
template <typename Check> void Core(Check&& check)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    OptionalSemanticExtensions(check);
    const std::wstring source = L"# Heading\n\nBefore **bold** & <em>native</em> [safe](https://example.test \"Title & more\") [unsafe](javascript:alert) "
                                L"![image](picture.png) $x^2$.\n\n"
                                L"5. first\n6. second\n\n| A | B |\n| - | - |\n| one | two |\n";
    const auto document       = RichSemanticDocumentTests::Build(source, MarkdownProfile::RichMarkdown);
    const auto content        = ExportSemanticDocument(*document);
    const auto html = content.HtmlFragment(), text = content.Text();
#if DXUI_ENABLE_DIAGNOSTICS
    std::optional<ExportContent> retained = content;
    const auto previousLimit              = DxUi::RichText::Detail::testRichExportUnitLimit;
    check(! text.empty() && html.size() > text.size(), "export fixture distinguishes text and HTML capacity paths");
    DxUi::RichText::Detail::testRichExportUnitLimit = 0u;
    const auto textCapacity                         = TryExportSemanticDocument(*document, {0u, document->Map().Display().size()}, retained);
    const bool textPreserved                        = retained && retained->Text() == text && retained->HtmlFragment() == html;
    DxUi::RichText::Detail::testRichExportUnitLimit = text.size();
    const auto htmlCapacity                         = TryExportSemanticDocument(*document, {0u, document->Map().Display().size()}, retained);
    const bool htmlPreserved                        = retained && retained->Text() == text && retained->HtmlFragment() == html;
    DxUi::RichText::Detail::testRichExportUnitLimit = previousLimit;
    check(textCapacity == SourceResult::CapacityExceeded && textPreserved, "text capacity refusal preserves the previously published export");
    check(htmlCapacity == SourceResult::CapacityExceeded && htmlPreserved, "HTML capacity refusal preserves the previously published export");
#endif
    const auto syntaxExport = ExportMarkdownSyntax(document->Syntax());
    check(syntaxExport.Text() == text && syntaxExport.HtmlFragment() == html, "immutable syntax exports through the default semantic projection");
    std::optional<ExportContent> checked = content;
    check(TryExportMarkdownSyntax(nullptr, checked) == SourceResult::InvalidSource && checked->Text() == text,
          "syntax export rejects an absent immutable publication without replacing output");
    check(html.find(L"<h1 id=\"heading\">Heading</h1>") != html.npos && html.find(L"<strong>bold</strong>") != html.npos &&
              html.find(L"<em>native</em>") != html.npos && html.find(L"&amp;") != html.npos,
          "semantic export preserves independent heading, inline style and escaping expectations");
    check(html.find(L"href=\"https://example.test\"") != html.npos && html.find(L"Title &amp; more") != html.npos && html.find(L"javascript:") == html.npos &&
              html.find(L"<img") == html.npos,
          "export links use escaped approved schemes and image alternatives load no resource");
    check(html.find(L"<ol start=\"5\">") != html.npos && html.find(L"<li>") != html.npos && html.find(L"<table>") != html.npos &&
              html.find(L"<th><strong>A</strong></th>") != html.npos && html.find(L"<td>one</td>") != html.npos,
          "semantic export retains authored list start and table header/body structure");
    check(text.find(L"Before bold & native safe unsafe [image: image] x^2.") != text.npos && text.find(L'\ufffc') == text.npos &&
              document->Syntax()->Source() == source,
          "rendered text replaces native math with a readable alternative without changing authored source");
    const auto display = document->Map().Display();
    const auto bold    = display.find(L"bold");
    const auto partial = ExportSemanticDocument(*document, {bold, bold + 4u});
    check(partial.Text() == L"bold" && partial.HtmlFragment() == L"<p><strong>bold</strong></p>\n",
          "a bounded display selection preserves its text, style and containing paragraph without exporting neighbors");
    const auto empty = ExportSemanticDocument(*document, {bold, bold});
    check(empty.Text().empty() && empty.HtmlFragment().empty(), "an empty display range exports no content");
    check(TryExportSemanticDocument(*document, {display.size(), display.size() + 1u}, checked) == SourceResult::InvalidRange && checked->Text() == text &&
              document->Syntax()->Source() == source,
          "out-of-range export preserves output and source publication");
    const auto second     = display.find(L"second");
    const auto secondItem = ExportSemanticDocument(*document, {second, second + 6u});
    check(secondItem.Text() == L"second" && secondItem.HtmlFragment().find(L"<ol start=\"6\">") != std::wstring_view::npos &&
              secondItem.HtmlFragment().find(L"first") == std::wstring_view::npos,
          "a partial ordered list retains the selected item's ordinal without exporting earlier items");
    for (const auto sourceText : {std::wstring_view(L"- alpha\n"), std::wstring_view(L"12. alpha\n")})
    {
        const auto list         = RichSemanticDocumentTests::Build(sourceText, MarkdownProfile::RichMarkdown);
        const auto listDisplay  = list->Map().Display();
        const auto contentStart = listDisplay.find(L"alpha");
        check(contentStart != listDisplay.npos && contentStart >= 2u, "list fixture has a generated multi-unit marker");
        if (contentStart == listDisplay.npos || contentStart < 2u)
            continue;
        for (const auto selection :
             {MarkdownRange{0u, 1u}, MarkdownRange{1u, contentStart}, MarkdownRange{1u, contentStart + 5u}, MarkdownRange{0u, contentStart - 1u}})
        {
            const auto selectedList = ExportSemanticDocument(*list, selection);
            const auto expected     = listDisplay.substr(selection.first, selection.last - selection.first);
            const auto fragment     = selectedList.HtmlFragment();
            check(selectedList.Text() == expected && fragment.starts_with(L"<pre>") && fragment.ends_with(L"</pre>\n") &&
                      fragment.substr(5u, fragment.size() - 12u) == expected && fragment.find(L"<li>") == fragment.npos,
                  "partial list markers export once as flat text, for marker-only and marker-plus-content ranges");
        }
    }
    const auto unsafe = ExportSemanticDocument(*RichSemanticDocumentTests::Build(L"<script>alert(1)</script>\n", MarkdownProfile::RichMarkdown));
    check(unsafe.HtmlFragment().find(L"<script") == unsafe.HtmlFragment().npos && unsafe.HtmlFragment().find(L"&lt;script&gt;") != unsafe.HtmlFragment().npos,
          "untrusted literal HTML never becomes an executable export tag");
    const auto rule = ExportSemanticDocument(*RichSemanticDocumentTests::Build(L"before\n\n***\n\nafter\n", MarkdownProfile::RichMarkdown));
    check(rule.HtmlFragment().find(L"<hr />") != rule.HtmlFragment().npos && rule.Text().find(L"---") != rule.Text().npos &&
              rule.Text().find(L'\ufffc') == rule.Text().npos,
          "a thematic rule exports as structure and a readable text alternative");
    {
        const auto navigationDocument = RichSemanticDocumentTests::Build(
            L"# Intro\n\n# Intro\n\n# Caf\u00e9\n\n[one](#intro) [two](#intro-1) [encoded](#%69ntro) [accent](#caf%C3%A9) [missing](#absent)\n",
            MarkdownProfile::RichMarkdown);
        const auto navigation = ExportSemanticDocument(*navigationDocument);
        const auto fragment   = navigation.HtmlFragment();
        check(fragment.find(L"<h1 id=\"intro\">Intro</h1>") != fragment.npos && fragment.find(L"<h1 id=\"intro-1\">Intro</h1>") != fragment.npos &&
                  fragment.find(L"<h1 id=\"caf\u00e9\">Caf\u00e9</h1>") != fragment.npos && fragment.find(L"href=\"#intro\"") != fragment.npos &&
                  fragment.find(L"href=\"#intro-1\"") != fragment.npos && fragment.find(L"href=\"#%69ntro\"") != fragment.npos &&
                  fragment.find(L"href=\"#caf%C3%A9\"") != fragment.npos && fragment.find(L"href=\"#absent\"") == fragment.npos,
              "local links use collision-resolved and Unicode heading ids; unresolved fragments stay inert text");
        const auto navDisplay        = navigationDocument->Map().Display();
        const auto start             = navDisplay.find(L"one");
        const auto partialNavigation = ExportSemanticDocument(*navigationDocument, {start, navDisplay.size()});
        check(partialNavigation.HtmlFragment().find(L"href=\"#intro\"") == std::wstring_view::npos &&
                  partialNavigation.HtmlFragment().find(L"one") != std::wstring_view::npos,
              "a selected fragment does not link to a heading omitted from that export");
    }
    for (const std::wstring_view sourceWithBreak : {L"first  \nsecond\n", L"first<br>second\n"})
    {
        const auto lineDocument = RichSemanticDocumentTests::Build(sourceWithBreak, MarkdownProfile::RichMarkdown);
        const auto lineExport   = ExportSemanticDocument(*lineDocument);
        check(lineExport.HtmlFragment().find(L"first<br />\nsecond") != lineExport.HtmlFragment().npos &&
                  lineExport.Text().find(L"first\nsecond") != lineExport.Text().npos,
              "hard and native HTML breaks remain visible in both exported formats");
        const auto lineDisplay  = lineDocument->Map().Display();
        const auto newline      = lineDisplay.find(L'\n');
        const auto partialBreak = ExportSemanticDocument(*lineDocument, {newline, newline + 1u});
        check(partialBreak.Text() == L"\n" && partialBreak.HtmlFragment().find(L"<br />") != partialBreak.HtmlFragment().npos,
              "a selection containing only a visible line break exports that break");
    }
    {
        const auto syntax = RichSemanticDocumentTests::Parse(L"first\nsecond\n", MarkdownProfile::RichMarkdown);
        SemanticBuilder builder;
        builder.SetSoftBreakPolicy(SoftBreakPolicy::LineBreak);
        builder.Begin(syntax);
        const auto documentWithBreak = RichSemanticDocumentTests::Complete(builder, *syntax);
        const auto lineExport        = ExportSemanticDocument(*documentWithBreak);
        check(lineExport.HtmlFragment().find(L"first<br />\nsecond") != lineExport.HtmlFragment().npos,
              "LineBreak soft-break policy is retained in rendered HTML");
    }
    const auto preformatted = ExportSemanticDocument(*RichSemanticDocumentTests::Build(L"```\nfirst\nsecond\n```\n", MarkdownProfile::RichMarkdown));
    check(preformatted.HtmlFragment().find(L"<pre><code>first\nsecond\n</code></pre>") != std::wstring_view::npos &&
              preformatted.HtmlFragment().find(L"<br") == std::wstring_view::npos,
          "preformatted source keeps literal newlines without HTML line-break tags");
    const auto unicode         = ExportSemanticDocument(*RichSemanticDocumentTests::Build(L"**\u6771\U0001f600** & text\n", MarkdownProfile::RichMarkdown));
    const auto unicodeDocument = RichSemanticDocumentTests::Build(L"**\u6771\U0001f600** & text\n", MarkdownProfile::RichMarkdown);
    const auto emoji           = unicodeDocument->Map().Display().find(L"\U0001f600");
    check(TryExportSemanticDocument(*unicodeDocument, {emoji + 1u, emoji + 2u}, checked) == SourceResult::InvalidRange,
          "display selection cannot split a surrogate pair");
    std::string encoded(0u, '\0');
    // Hand-counted wire bytes, independent of the SDK's decoder and offset
    // formatter. The fragment contains both three- and four-byte UTF-8 scalars.
    constexpr std::string_view golden = "Version:1.0\r\nStartHTML:0000000105\r\nEndHTML:0000000183\r\nStartFragment:0000000137\r\nEndFragment:0000000151\r\n"
                                        "<html><body><!--StartFragment--><b>\xe6\x9d\xb1\xf0\x9f\x98\x80</b><!--EndFragment--></body></html>";
    check(EncodeClipboardHtml(L"<b>\u6771\U0001f600</b>", encoded) == S_OK && encoded == golden,
          "CF_HTML encoder matches independent byte offsets, fragment markers, context and UTF-8 wire bytes");
    // Original expected wire vector from Chromium's producer test (BSD):
    // https://github.com/chromium/chromium/blob/ec385701fa11cffe4bb8102fa78cee741f3b774a/ui/base/clipboard/clipboard_util_win_unittest.cc
    // Blob c2b682c557f115b85106de4acda74fd4445213a4; no SDK encoder generates this input.
    constexpr std::string_view producerFragment = "<html><head><style>p {color:blue}</style></head><body><p>Hello World</p></body></html>";
    constexpr std::string_view producerWire =
        "Version:0.9\r\nStartHTML:0000000132\r\nEndHTML:0000000290\r\nStartFragment:0000000168\r\nEndFragment:0000000254\r\n"
        "SourceURL:www.example.com\r\n<html>\r\n<body>\r\n<!--StartFragment-->"
        "<html><head><style>p {color:blue}</style></head><body><p>Hello World</p></body></html>"
        "<!--EndFragment-->\r\n</body>\r\n</html>";
    std::wstring producerDecoded;
    check(DecodeClipboardHtml(std::as_bytes(std::span(producerWire.data(), producerWire.size())), producerDecoded) == S_OK &&
              producerDecoded == std::wstring(producerFragment.begin(), producerFragment.end()),
          "independent Chromium producer bytes handle a SourceURL header and distinct context/fragment offsets");
    auto splitScalar = std::string(golden);
    splitScalar.replace(splitScalar.find("StartFragment:") + 14u, 10u, "0000000141");
    producerDecoded = L"retained fragment";
    check(FAILED(DecodeClipboardHtml(std::as_bytes(std::span(splitScalar.data(), splitScalar.size())), producerDecoded)) &&
              producerDecoded == L"retained fragment",
          "a byte offset inside the independently placed UTF-8 scalar cannot publish partial HTML");
    check(EncodeClipboardHtml(unicode.HtmlFragment(), encoded) == S_OK, "Unicode CF_HTML encoding succeeds");
    const auto encodedExport = encoded;
    std::wstring decoded(0u, L' ');
    check(DecodeClipboardHtml(std::as_bytes(std::span(encoded.data(), encoded.size())), decoded) == S_OK && decoded == unicode.HtmlFragment(),
          "CF_HTML byte offsets round-trip multibyte UTF-8 and supplementary characters");
    encoded.assign("sentinel");
    check(EncodeClipboardHtml(std::wstring(1u, wchar_t(0xd800)), encoded) == E_INVALIDARG && encoded == "sentinel",
          "invalid UTF-16 leaves staged clipboard output untouched");
    // Private clipboard fixture; this synthetic handle never reaches Win32
    // ownership calls and the desktop clipboard is never read or modified.
    const auto owner = reinterpret_cast<HWND>(std::uintptr_t{1u});
    ClipboardTransportFixture clipboard(owner, RichSemanticTests::Check);
    auto& transport = SystemTextClipboard();
    check(CopyExportToClipboard(owner, unicode) == S_OK && clipboard.allocations == 2u && clipboard.opens == 1u && clipboard.closes == 1u &&
              clipboard.publications == 2u && clipboard.data && clipboard.htmlData,
          "rendered copy stages both formats before one balanced clipboard ownership attempt");
    const bool memoryFits = clipboard.htmlData && GlobalSize(clipboard.htmlData.get()) >= encodedExport.size() + 1u;
    check(clipboard.lastAllocationBytes == encodedExport.size() + 1u && memoryFits,
          "HTML publication requests the wire bytes plus exactly one NUL, allowing native allocator rounding");
    const auto* published = memoryFits ? static_cast<const char*>(GlobalLock(clipboard.htmlData.get())) : nullptr;
    check(published != nullptr, "lock the independently owned HTML publication");
    if (published)
    {
        const auto unlock = wil::scope_exit([&]() noexcept { GlobalUnlock(clipboard.htmlData.get()); });
        check(std::string_view(published, encodedExport.size()) == encodedExport && published[encodedExport.size()] == '\0',
              "published HGLOBAL contains exact wire bytes and its terminal NUL");
    }
    std::wstring copied(0u, L' ');
    check(transport.Read(owner, copied) == S_OK && copied == unicode.Text(), "rendered Unicode is the copied fallback format");
    clipboard.failAllocation = true;
    const auto previousOpens = clipboard.opens;
    check(CopyExportToClipboard(owner, content) == E_OUTOFMEMORY && clipboard.opens == previousOpens,
          "rendered-copy allocation failure preserves the existing clipboard before ownership");
    clipboard.failAllocation = false;
    clipboard.busy           = true;
    check(FAILED(CopyExportToClipboard(owner, content)) && clipboard.opens == previousOpens + 1u, "busy rendered copy attempts ownership exactly once");
    clipboard.busy            = false;
    clipboard.failHtmlPublish = true;
    check(FAILED(CopyExportToClipboard(owner, content)) && ! clipboard.htmlData && transport.Read(owner, copied) == S_OK && copied == content.Text(),
          "partial OS format publication remains explicit failure with valid transferred Unicode ownership");
    struct LegacyClipboard final : TextClipboard
    {
        std::wstring copiedText = std::wstring(0u, L' ');
        HRESULT Read(HWND, std::wstring&) noexcept override
        {
            return E_NOTIMPL;
        }
        HRESULT Write(HWND, std::wstring_view value) noexcept override
        try
        {
            copiedText.assign(value);
            return S_OK;
        }
        catch (const std::bad_alloc&)
        {
            DxUi::Failure::FailAllocation();
        }
    } legacy;
    check(CopyExportToClipboard(owner, content, &legacy) == S_OK && legacy.copiedText == content.Text(),
          "an unchanged legacy clipboard adapter receives rendered text through its original virtual");
}
} // namespace RichTextExportTests
