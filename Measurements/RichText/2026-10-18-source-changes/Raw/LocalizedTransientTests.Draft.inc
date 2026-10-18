void TestRichSourceChangeLocalizedTransientPreviews()
{
    Document document(L"abcdef");
    const auto original=document.Snapshot();
    document.ReplaceTransient(L"abXYZef",{5u,5u});
    CheckTracked(original,document.Snapshot(),2u,4u,5u);
    const auto current=document.Snapshot();
    const auto revision=document.Revision();
    Require(!document.ReplaceTransient(L"abXYZef",{5u,2u}) && document.Revision()==revision &&
                document.Snapshot().ChangedRangeFrom(current).kind==SourceChangeKind::Unchanged,
            "same-content preview changes neither revision nor metadata");
    Document crlf(L"A\rx\nB");
    const auto beforeCrlf=crlf.Snapshot();
    crlf.ReplaceTransient(L"A\r\nB",{3u,3u});
    CheckTracked(beforeCrlf,crlf.Snapshot(),1u,4u,3u);
    for(const auto replacement:{L"A\U0001f603B",L"A\U00020200B"})
    {
        Document scalar(L"A\U0001f600B");
        const auto before=scalar.Snapshot();
        scalar.ReplaceTransient(replacement,{3u,3u});
        CheckTracked(before,scalar.Snapshot(),1u,3u,3u);
    }
    document.ReplaceTransient(L"",{0u,0u});
    Require(document.Text().empty(),"localized preview can remove every source unit");
    const auto empty=document.Snapshot();
    document.ReplaceTransient(L"\U0001f600\r\n",{4u,4u});
    CheckTracked(empty,document.Snapshot(),0u,0u,4u);
    Require(!document.CanUndo() && !document.CanRedo(),"localized previews never create ordinary history");
}
