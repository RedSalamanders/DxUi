struct LayoutPreparation
{
    std::size_t preparedChunks{}, reusedChunks{}, preparedSourceUnits{};
};
// Owner-thread, bounded v0 document geometry. Chunks end at physical source
// lines and target 1 Ki source units; no document retains more than 128 chunks.
// Source edits may retain immutable old chunks for the next Prepare. Invalidate
// rejects stale geometry; Clear additionally releases the complete retained graph.
// This service owns no HWND, graphics device, worker, timer or scheduling policy.
class PreparedDocumentLayout
{
public:
    PreparedDocumentLayout() noexcept;
    ~PreparedDocumentLayout() noexcept;
    PreparedDocumentLayout(const PreparedDocumentLayout&) = delete;
    PreparedDocumentLayout& operator=(const PreparedDocumentLayout&) = delete;
    void Prepare(IDWriteFactory* factory, SourceSnapshot source, std::uint64_t revision, ViewMode mode, const LayoutOptions& options);
    void Invalidate() noexcept;
    void Clear() noexcept;
    bool Ready() const noexcept;
    float Height() const noexcept;
    std::uint64_t Generation() const noexcept;
    std::uint64_t Revision() const noexcept;
    LayoutPreparation LastPreparation() const noexcept;
    std::size_t ChunkCount() const noexcept;
    std::size_t DisplayLength() const;
    std::size_t SourceLength() const;
    wchar_t DisplayUnit(std::size_t position) const;
    bool SourceMatches(std::wstring_view source) const;
    // Earliest source ACP whose display mapping reaches this boundary. Used to
    // partition visual lines including their original hidden delimiters/EOLs.
    std::size_t SourceBoundary(std::size_t displayOffset) const;
    std::size_t ToDisplay(std::size_t sourceOffset) const;
    std::size_t ToSource(std::size_t displayOffset, bool trailing = false) const;
    std::wstring DisplayText() const;
    Hit HitTest(float x, float y) const;
    CaretGeometry Caret(std::size_t position) const;
    std::vector<D2D1_RECT_F> Range(std::size_t first, std::size_t last) const;
    std::size_t PreviousCluster(std::size_t position) const;
    std::size_t NextCluster(std::size_t position) const;
    std::pair<std::size_t, std::size_t> DeletionRange(std::size_t position, bool backwards) const;
    std::size_t LineBoundary(std::size_t position, bool end) const;
    std::pair<std::size_t, std::size_t> VisibleRange(float top, float bottom) const;
    // Raster preparation only. A viewport limits draw calls to intersecting
    // chunks; composition consumes the host's existing cached surface.
    void Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, float top, float bottom) const;

private:
    struct State;
    const State& Prepared() const;
    std::size_t ChunkAt(std::size_t position) const;
    std::unique_ptr<State> state_;
    std::uint64_t generation_{};
    bool ready_{};
};
