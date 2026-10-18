bool Document::ReplaceTransient(std::wstring_view text,Selection selection)
{
    Validate(text);
    const auto length=Length();
    const auto commonLimit=std::min(length,text.size());
    size_t prefix=0u;
    SourceSnapshot::PieceTreeNode::VisitRange(source_.RootNode(),0u,commonLimit,[&](std::wstring_view fragment)
    {
        if(fragment==text.substr(prefix,fragment.size()))
        {
            prefix+=fragment.size();
            return true;
        }
        const auto mismatch=std::mismatch(fragment.begin(),fragment.end(),text.begin()+static_cast<std::ptrdiff_t>(prefix));
        const auto matched=static_cast<size_t>(mismatch.first-fragment.begin());
        prefix+=matched;
        return matched==fragment.size();
    });
    while(prefix && (!source_.IsBoundary(prefix) || !IsEditBoundary(text,prefix)))--prefix;
    size_t suffix=0u;
    const auto suffixLimit=commonLimit-prefix;
    SourceSnapshot::PieceTreeNode::VisitRangeReverse(source_.RootNode(),length-suffixLimit,length,[&](std::wstring_view fragment)
    {
        if(fragment==text.substr(text.size()-suffix-fragment.size(),fragment.size()))
        {
            suffix+=fragment.size();
            return true;
        }
        const auto targetEnd=text.begin()+static_cast<std::ptrdiff_t>(text.size()-suffix);
        const auto mismatch=std::mismatch(fragment.rbegin(),fragment.rend(),std::make_reverse_iterator(targetEnd));
        const auto matched=static_cast<size_t>(mismatch.first-fragment.rbegin());
        suffix+=matched;
        return matched==fragment.size();
    });
    while(suffix && (!source_.IsBoundary(length-suffix) || !IsEditBoundary(text,text.size()-suffix)))--suffix;
    // The OS supplies whole preview text. Retain only its changed interval;
    // untouched buffers/paths and useful incremental metadata stay shared.
    return ReplaceImpl(prefix,length-suffix,text.substr(prefix,text.size()-prefix-suffix),selection,false);
}
