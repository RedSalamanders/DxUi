SourceChangeRange SourceSnapshot::ChangedRangeFrom(const SourceSnapshot& previous) const noexcept
{
    const auto previousLength=previous.Length(), currentLength=Length();
    if(state_==previous.state_ || (!previousLength && !currentLength))
        return {SourceChangeKind::Unchanged,0u,0u,0u};
    const SourceChangeRange fallback{SourceChangeKind::FullReplacement,0u,previousLength,currentLength};
    if(!state_ || !previous.state_ || state_->lineage!=previous.state_->lineage)
        return fallback;
    constexpr size_t maximumEdits=SnapshotEditPage::Capacity*4u;
    std::array<SnapshotEdit,maximumEdits> currentEdits{}, previousEdits{};
    const auto currentCount=state_->CopyNewestEdits(currentEdits);
    const auto previousCount=previous.state_->CopyNewestEdits(previousEdits);
    size_t currentSteps=0u, previousSteps=0u;
    bool found=false;
    // Numeric ancestry survives expired transient preview objects without
    // retaining their source buffers. Search is bounded to two small windows.
    for(size_t oldStep=0u; oldStep<=previousCount && !found; ++oldStep)
    {
        const auto oldIdentity=oldStep?previousEdits[oldStep-1u].previousIdentity:previous.state_->identity;
        for(size_t newStep=0u; newStep<=currentCount; ++newStep)
        {
            const auto newIdentity=newStep?currentEdits[newStep-1u].previousIdentity:state_->identity;
            if(oldIdentity==newIdentity)
            {
                previousSteps=oldStep;
                currentSteps=newStep;
                found=true;
                break;
            }
        }
    }
    if(!found)return fallback;
    size_t begin=0u, previousEnd=0u;
    bool hasEdit=false;
    const auto mapBack=[](size_t position,const SnapshotEdit& edit,bool trailing)noexcept
    {
        if(position<edit.begin)return position;
        if(position>edit.currentEnd)return position-edit.currentEnd+edit.previousEnd;
        return trailing?edit.previousEnd:edit.begin;
    };
    const auto accumulate=[&](const SnapshotEdit& edit)noexcept
    {
        if(!hasEdit)
        {
            begin=edit.begin;
            previousEnd=edit.previousEnd;
            hasEdit=true;
        }
        else
        {
            begin=std::min(mapBack(begin,edit,false),edit.begin);
            previousEnd=std::max(mapBack(previousEnd,edit,true),edit.previousEnd);
        }
    };
    // Walk the combined previous -> ancestor -> current script backwards.
    for(size_t index=0u; index<currentSteps; ++index)
        accumulate(currentEdits[index]);
    for(size_t index=previousSteps; index>0u; --index)
    {
        auto inverse=previousEdits[index-1u];
        std::swap(inverse.previousEnd,inverse.currentEnd);
        accumulate(inverse);
    }
    if(!hasEdit || previousEnd>previousLength)return fallback;
    const auto suffixLength=previousLength-previousEnd;
    if(suffixLength>currentLength)return fallback;
    auto currentEnd=currentLength-suffixLength;
    if(begin>previousEnd || begin>currentEnd)return fallback;
    // Raw deltas can meet inside a newly joined CRLF. Expand matching unchanged
    // context in both coordinate spaces so callers can safely use Slice.
    while(begin && (!previous.IsBoundary(begin) || !IsBoundary(begin)))--begin;
    while(previousEnd<previousLength && currentEnd<currentLength && (!previous.IsBoundary(previousEnd) || !IsBoundary(currentEnd)))
    {
        ++previousEnd;
        ++currentEnd;
    }
    if(!previous.IsBoundary(previousEnd) || !IsBoundary(currentEnd))return fallback;
    return {SourceChangeKind::TrackedEdits,begin,previousEnd,currentEnd};
}
