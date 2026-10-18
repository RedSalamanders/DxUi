namespace
{
struct SnapshotEdit
{
    std::uint64_t previousIdentity{}, currentIdentity{};
    size_t begin{}, previousEnd{}, currentEnd{};
};
struct SnapshotEditPage
{
    static constexpr size_t Capacity = 32u;
    std::array<SnapshotEdit, Capacity> entries{};
};
struct SnapshotLineage
{
    std::atomic<std::uint64_t> nextIdentity{1u};
    std::uint64_t AllocateIdentity()
    {
        auto value = nextIdentity.load(std::memory_order_relaxed);
        do
        {
            if(value == std::numeric_limits<std::uint64_t>::max())
                throw std::length_error("Source snapshot identity space exhausted");
        }while(!nextIdentity.compare_exchange_weak(value,value+1u,std::memory_order_relaxed));
        return value;
    }
};
} // namespace
struct SourceSnapshot::Revision
{
    using Pointer = std::shared_ptr<const Revision>;
    std::shared_ptr<const PieceTreeNode> root;
    std::shared_ptr<SnapshotLineage> lineage;
    std::uint64_t identity{};
    // Sealed pages contain numeric edits only: no source root or page chain.
    // Three pages plus the active 1..32 edits retain a bounded 97..128 window.
    std::array<std::shared_ptr<const SnapshotEditPage>,3u> sealed;
    SnapshotEditPage active;
    size_t activeCount{};
    Revision(std::shared_ptr<const PieceTreeNode> tree,const Revision* previous,size_t begin,size_t previousEnd,size_t currentEnd)
        :root(std::move(tree)),lineage(previous?previous->lineage:std::make_shared<SnapshotLineage>()),identity(lineage->AllocateIdentity())
    {
        if(!previous)
            return; // Reset starts an unrelated lineage, including empty source.
        sealed=previous->sealed;
        if(previous->activeCount == SnapshotEditPage::Capacity)
        {
            auto completed=std::make_shared<const SnapshotEditPage>(previous->active);
            sealed[0]=std::move(sealed[1]);
            sealed[1]=std::move(sealed[2]);
            sealed[2]=std::move(completed);
        }
        else
        {
            active=previous->active;
            activeCount=previous->activeCount;
        }
        active.entries[activeCount++]={previous->identity,identity,begin,previousEnd,currentEnd};
    }
    size_t CopyNewestEdits(std::span<SnapshotEdit> output)const noexcept
    {
        size_t used=0u;
        for(size_t index=activeCount; index>0u && used<output.size(); --index)
            output[used++]=active.entries[index-1u];
        for(size_t page=sealed.size(); page>0u; --page)
            if(sealed[page-1u])
                for(size_t index=SnapshotEditPage::Capacity; index>0u && used<output.size(); --index)
                    output[used++]=sealed[page-1u]->entries[index-1u];
        return used;
    }
};
