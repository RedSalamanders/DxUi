#pragma once
#include "../../src/Controls/DxUi.RichImagePaths.h"
#include "../../src/Controls/TextClipboard.h"
#include <DxUi/DxUi.h>
#include <cstring>
#include <shlobj.h>
namespace RichImagePathTests
{
template <typename Check> inline void Paths(Check&& check)
{
    using namespace DxUi::RichText::Detail;
    struct Case
    {
        const wchar_t* document;
        const wchar_t* clipboard;
        const wchar_t* expected;
    };
    const Case cases[] = {
        {L"C:\\docs\\note.md", L"C:\\docs\\photo.png", L"![photo](photo.png)"},
        {L"C:\\docs\\note.md", L"\"C:\\docs\\media\\a [b]#%.PNG\"\r\n", L"![a \\[b\\]\\#\\%](media/a%20%5Bb%5D%23%25.PNG)"},
        {L"C:\\docs\\chapters\\note.md", L"C:\\docs\\a.png", L"![a](../a.png)"},
        {L"C:\\docs\\note.md", L"C:\\images\\a.png", L"![a](../images/a.png)"},
        {L"C:\\DOCS\\note.md", L"c:\\docs\\a.png", L"![a](../docs/a.png)"},
        {L"C:\\docs\\note.md", L"D:\\images\\a.png", L"![a](file:///D:/images/a.png)"},
        {L"", L"C:\\images\\a.png", L"![a](file:///C:/images/a.png)"},
        {L"C:\\docs\\note.md", L"C:\\docs\\\u6771\U0001f600.png", L"![\u6771\U0001f600](%E6%9D%B1%F0%9F%98%80.png)"},
        {L"\\\\server\\share\\docs\\note.md", L"\\\\SERVER\\share\\a.png", L"![a](../a.png)"},
        {L"\\\\server\\share\\note.md", L"\\\\server\\other\\a.png", L"![a](file://server/other/a.png)"},
        {L"C:\\docs\\note.md", L"\\\\?\\C:\\docs\\a.png", L"![a](a.png)"},
        {L"C:\\docs\\note.md", L"C:/docs/other/../a.png", L"![a](a.png)"},
    };
    for (const auto& item : cases)
    {
        const auto result = BuildMarkdownImagePath(item.document, item.clipboard);
        check(result && *result == item.expected, "image paths use escaped source and document-relative URI destinations");
    }
    for (const auto* invalid :
         {L"a.png", L"C:a.png", L"\\a.png", L"C:\\a.txt", L"C:\\a.png\nC:\\b.png", L"\\\\.\\C:\\a.png", L"C:\\..\\a.png", L"C:\\a.png:stream"})
        check(! BuildMarkdownImagePath(L"C:\\note.md", invalid), "ambiguous/non-image/device paths remain ordinary Unicode text");
    check(! BuildMarkdownImagePath(L"", std::wstring(1u, wchar_t(0xd800))), "malformed UTF-16 is never interpreted as an image path");
    for (const auto* invalid :
         {L"C:\\NUL.png", L"C:\\com1.jpg", L"C:\\COM\u00b9.png", L"C:\\folder.\\a.png", L"C:\\folder \\a.png", L"\\\\?\\C:\\folder\\..\\a.png"})
        check(! BuildMarkdownImagePath(L"C:\\note.md", invalid), "DOS devices and ambiguous extended/trailing component spellings remain literal");
    struct ResolveCase
    {
        const wchar_t* document;
        const wchar_t* destination;
        const wchar_t* path;
    };
    const ResolveCase destinations[] = {
        {L"C:\\docs\\note.md", L"media/a%20%23.png", L"C:\\docs\\media\\a #.png"},
        {L"C:\\docs\\note.md", L"../\u6771%F0%9F%98%80.png", L"C:\\\u6771\U0001f600.png"},
        {L"", L"file:///D:/images/a.png", L"D:\\images\\a.png"},
        {L"", L"file://localhost/C:/a.png", L"C:\\a.png"},
        {L"", L"file://server/share/a.png", L"\\\\server\\share\\a.png"},
        {L"C:\\note.md", L"a%2520.png", L"C:\\a%20.png"},
    };
    for (const auto& item : destinations)
    {
        const auto result = ResolveLocalImagePath(item.document, item.destination);
        check(result && *result == item.path, "local image URI resolution preserves exact decoded Unicode and explicit base/root");
    }
    for (const auto* invalid : {L"https://example.invalid/a.png",
                                L"data:image/png;base64,x",
                                L"file:relative.png",
                                L"file://localhost/images/a.png",
                                L"a.png?x",
                                L"a.png#x",
                                L"%2f%2fserver/share/a.png",
                                L"a%5cb.png",
                                L"a%00.png",
                                L"a%FF.png",
                                L"a%2.png",
                                L"/a.png"})
        check(! ResolveLocalImagePath(L"C:\\docs\\note.md", invalid),
              "remote/query/fragment/encoded separators/malformed URI paths never become local requests");
    check(! ResolveLocalImagePath(L"", L"a.png") && ! ResolveLocalImagePath(L"C:\\note.md", L"../a.png"),
          "unsaved relative and above-root image destinations remain unresolved");
    check(SameLocalImagePath(L"C:\\docs\\a.png", L"c:\\docs\\a.png") && SameLocalImagePath(L"\\\\SERVER\\share\\a.png", L"\\\\server\\share\\a.png") &&
              ! SameLocalImagePath(L"C:\\Docs\\a.png", L"C:\\docs\\a.png"),
          "resource keys coalesce Windows roots while preserving case-sensitive file/directory spelling");
    check(NormalizeDocumentFilePath(L"C:/docs/../note.md") == L"C:\\note.md", "document metadata uses a lexical canonical absolute spelling");
    try
    {
        static_cast<void>(BuildMarkdownImagePath(L"", L"C:\\" + std::wstring(22000u, L'\u6771') + L".png"));
        check(false, "encoded source capacity must fail");
    }
    catch (const std::length_error&)
    {
    }
}
inline std::vector<std::byte> DropBuffer(std::wstring_view list)
{
    std::vector<std::byte> bytes(sizeof(DROPFILES) + list.size() * sizeof(wchar_t));
    const DROPFILES header{.pFiles = sizeof(DROPFILES), .fWide = TRUE};
    std::memcpy(bytes.data(), &header, sizeof(header));
    std::memcpy(bytes.data() + sizeof(header), list.data(), list.size() * sizeof(wchar_t));
    return bytes;
}
template <typename Check> inline void Codec(Check&& check)
{
    using namespace DxUi;
    constexpr wchar_t list[] = L"C:\\a.png\0D:\\\u6771\U0001f600.jpg\0";
    const auto good          = DropBuffer(std::wstring_view(list, std::size(list)));
    TextClipboardContent output;
    output.text = L"sentinel";
    check(DecodeClipboardFilePaths(good, output) == S_OK && output.text.empty() && output.filePaths.size() == 2u &&
              output.filePaths[1] == L"D:\\\u6771\U0001f600.jpg",
          "wide CF_HDROP preserves ordered Unicode file names");
    for (unsigned variant = 0; variant < 6u; ++variant)
    {
        auto bad = good;
        DROPFILES header{};
        std::memcpy(&header, bad.data(), sizeof(header));
        if (variant == 0u)
            header.fWide = FALSE;
        if (variant == 1u)
            header.pFiles = 0u;
        if (variant == 2u)
            header.pFiles = static_cast<DWORD>(bad.size() + 2u);
        if (variant == 3u)
            header.pFiles = sizeof(header) + 1u;
        std::memcpy(bad.data(), &header, sizeof(header));
        if (variant == 4u)
            bad.resize(bad.size() - sizeof(wchar_t));
        if (variant == 5u)
        {
            const wchar_t invalid = 0xd800;
            std::memcpy(bad.data() + sizeof(header), &invalid, sizeof(invalid));
        }
        output.text = L"sentinel";
        output.filePaths.clear();
        check(FAILED(DecodeClipboardFilePaths(bad, output)) && output.text == L"sentinel" && output.filePaths.empty(),
              "malformed/ANSI/unterminated file lists preserve caller output");
    }
    constexpr wchar_t empty[] = {0, 0};
    check(DecodeClipboardFilePaths(DropBuffer(std::wstring_view(empty, 2u)), output) == S_FALSE && output.text == L"sentinel",
          "empty file lists request Unicode fallback without publication");
    std::wstring many(0u, L' ');
    for (unsigned i = 0; i < 17u; ++i)
        many.append(L"C:\\a.png").push_back(0);
    many.push_back(0);
    check(DecodeClipboardFilePaths(DropBuffer(many), output) == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW) && output.text == L"sentinel",
          "file-count capacity fails before publication");
}
template <typename Prepare, typename Check> inline void Retained(DxUi::RichTextEditor& editor, Prepare&& prepare, Check&& check)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    editor.SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor.SetMarkdownRenderingEnabled(true);
    editor.SetText(L"begin\n");
    editor.SetSelection({6u, 6u});
    prepare();
    const auto revision  = editor.Revision();
    const auto selection = editor.GetSelection();
    editor.SetDocumentFilePath(L"C:/docs/note.md");
    check(editor.GetDocumentFilePath() == L"C:\\docs\\note.md" && editor.Revision() == revision && editor.GetSelection() == selection && ! editor.CanUndo(),
          "document location is metadata with no source or history transaction");
    prepare();
    TextClipboardContent content;
    content.text             = L"\"C:\\docs\\media\\a #.png\"";
    std::wstring replacement = L"sentinel";
    check(editor.TryPrepareClipboardPaste(content, replacement) && replacement == L"![a \\#](media/a%20%23.png)" && editor.Revision() == revision,
          "image paste preparation is pure and emits Markdown");
    check(editor.ReplaceSelection(replacement) && editor.GetText() == L"begin\n![a \\#](media/a%20%23.png)", "prepared image commits once as source");
    prepare();
    const auto semantic = editor.GetSemanticDocument();
    check(static_cast<bool>(semantic), "image paste prepares current semantic source");
    const auto image = std::ranges::find(semantic->Elements(), SemanticRole::Image, &SemanticElement::role);
    check(image != semantic->Elements().end() && image->name == L"a #" && image->destination == L"media/a%20%23.png",
          "emitted source independently parses into the intended image alternative and URI");
    check(editor.Undo() && editor.GetText() == L"begin\n" && editor.GetSelection() == selection && ! editor.CanUndo(),
          "one undo restores complete image insertion and selection");
    check(editor.Redo() && editor.GetText().ends_with(replacement), "image insertion is redoable");
    editor.SetText(L"\n");
    editor.SetSelection({1u, 1u});
    prepare();
    content.filePaths = {L"C:\\docs\\a.png", L"C:\\docs\\b.jpg"};
    check(editor.TryPrepareClipboardPaste(content, replacement) && replacement == L"![a](a.png)\n\n![b](b.jpg)",
          "multiple images remain distinct Markdown paragraphs in source EOL style");
    content.filePaths.push_back(L"C:\\docs\\notes.txt");
    check(editor.TryPrepareClipboardPaste(content, replacement) && replacement == L"\"C:\\docs\\a.png\"\n\"C:\\docs\\b.jpg\"\n\"C:\\docs\\notes.txt\"",
          "mixed file lists preserve every original path as literal source");
    content.filePaths.clear();
    content.text = L"C:\\docs\\a.png";
    for (const auto* text :
         {L"```\nraw\n```\n", L"    code\n", L"<script>raw</script>\n", L"---\nname: value\n---\n", L"`inline code`", L"[link](url)", L"![image](url)"})
    {
        editor.SetText(text);
        editor.SetSelection({5u, 5u});
        prepare();
        replacement = L"sentinel";
        check(! editor.TryPrepareClipboardPaste(content, replacement) && replacement == L"sentinel",
              "opaque code/HTML/metadata contexts preserve literal pasted text");
    }
    editor.SetText(L"ok");
    editor.SetSelection({2u, 2u});
    prepare();
    editor.SetViewMode(ViewMode::Source);
    replacement = L"sentinel";
    check(! editor.TryPrepareClipboardPaste(content, replacement) && replacement == L"sentinel", "source mode keeps literal clipboard source");
    editor.SetViewMode(ViewMode::Rich);
    prepare();
    editor.SetReadOnly(true);
    check(! editor.TryPrepareClipboardPaste(content, replacement), "read-only image paste is rejected");
    editor.SetReadOnly(false);
    editor.SetEnabled(false);
    check(! editor.TryPrepareClipboardPaste(content, replacement), "disabled image paste is rejected");
    editor.SetEnabled(true);
}
} // namespace RichImagePathTests
