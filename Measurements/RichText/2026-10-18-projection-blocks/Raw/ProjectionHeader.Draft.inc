// One physical v0 source line, including its line ending. Projection coordinates
// are local to the block; prefixes place it in the current prepared document.
// The immutable projection may be retained across revisions by a caller.
struct ProjectionBlock
{
    std::size_t sourceStart{}, displayStart{};
    std::shared_ptr<const Projection> content;
};
struct ProjectionPreparation
{
    std::size_t rebuiltBlocks{}, reusedBlocks{}, rebuiltSourceUnits{};
};
// Bounded v0 projection preparation. It reuses unchanged physical-line blocks;
// it is not yet a CommonMark block parser or a geometry/scheduling service.
// Preparation stages all changed blocks/indexes before publishing. Call on the
// owner's preparation thread; it owns no worker, device, HWND or timer.
class PreparedProjection
{
public:
    PreparedProjection() noexcept;
    ~PreparedProjection() noexcept;
    PreparedProjection(PreparedProjection&&) noexcept;
    PreparedProjection& operator=(PreparedProjection&&) noexcept;
    PreparedProjection(const PreparedProjection&) = delete;
    PreparedProjection& operator=(const PreparedProjection&) = delete;
    void Prepare(SourceSnapshot source, std::uint64_t revision, ViewMode mode);
    void Clear() noexcept;
    bool Ready() const noexcept;
    std::uint64_t Revision() const noexcept;
    ViewMode Mode() const noexcept;
    std::size_t SourceLength() const noexcept;
    std::size_t DisplayLength() const noexcept;
    const std::vector<ProjectionBlock>& Blocks() const;
    ProjectionPreparation LastPreparation() const noexcept;
    std::size_t ToDisplay(std::size_t sourceOffset) const;
    std::size_t ToSource(std::size_t displayOffset, bool trailing = false) const;
    // Explicit materialization for callers that need a flat copy. Normal block
    // preparation/mapping does not flatten the complete source or display text.
    std::wstring DisplayText() const;

private:
    struct State;
    std::unique_ptr<State> state_;
};
