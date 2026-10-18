void TestRichDocumentBorrowedInputLifetime()
{
    std::wstring input = L"A\U0001f600\r\nB";
    const auto initial = input;
    Document document{std::wstring_view(input)};
    input.assign(2048u, L'x');
    Require(document.Text() == initial, "construction copies borrowed Unicode source before returning");
    const auto initialSnapshot = document.Snapshot();
    {
        std::wstring replacement = L"prefix\u0915\u094d\u0915suffix";
        document.Replace(0u, document.Length(), std::wstring_view(replacement).substr(6u, 3u), {3u, 3u});
        replacement.assign(4096u, L'z');
    }
    Require(document.Text() == L"\u0915\u094d\u0915" && initialSnapshot.Text() == initial,
            "replacement copies the borrowed substring while earlier snapshots remain immutable");
    document.SetSelection({0u, document.Length()});
    document.ReplaceSelection(std::wstring(L"\U0001f469\u200d\U0001f4bb"));
    Require(document.Text() == L"\U0001f469\u200d\U0001f4bb", "selection replacement outlives a temporary owning argument");
    document.Undo();
    const bool undo = document.CanUndo(), redo = document.CanRedo();
    {
        std::wstring preview = L"transient\r\n\U0001f600";
        document.ReplaceTransient(std::wstring_view(preview), {0u, 0u});
        preview.clear();
    }
    Require(document.Text() == L"transient\r\n\U0001f600" && document.CanUndo() == undo && document.CanRedo() == redo,
            "transient replacement owns its borrowed source and preserves both histories");
    {
        std::wstring reset = L"reset\r\n\U0001f600";
        document.Reset(std::wstring_view(reset));
        reset.assign(1024u, L'q');
    }
    Require(document.Text() == L"reset\r\n\U0001f600" && !document.CanUndo() && !document.CanRedo(),
            "reset owns the borrowed source and retires history only after success");
    const auto revision = document.Revision();
    const auto same = document.Text();
    Require(!document.Replace(0u, document.Length(), std::wstring_view(same), {document.Length(), 0u}) &&
                document.GetSelection() == Selection{document.Length(), 0u} && document.Revision() == revision && same == document.Text(),
            "same-content view acknowledges selection without consuming caller storage");
}
