// Inactive implementation draft for the next, separately measured slice.
// State is private and forward declared by SourceSnapshot's public header.
struct SourceSnapshot::State
{
    using Pointer = std::shared_ptr<const State>;
    static constexpr size_t Capacity = 32u, Minimum = Capacity / 2u;
    struct Fragment
    {
        std::shared_ptr<const std::wstring> buffer;
        size_t bufferStart{}, length{};
    };
    struct Piece
    {
        std::shared_ptr<const std::wstring> buffer;
        size_t bufferStart{}, sourceEnd{};
    };
    struct Child
    {
        Pointer node;
        size_t sourceEnd{};
    };
    using Leaves = std::array<Piece, Capacity>;
    using Branches = std::array<Child, Capacity>;
    struct Result
    {
        Pointer first, second;
    };
    std::variant<Leaves, Branches> entries;
    size_t length{}, count{}, height{};

    explicit State(std::span<const Fragment> fragments) : entries(Leaves{}), count(fragments.size())
    {
        auto& pieces = std::get<Leaves>(entries);
        for (size_t index = 0u; index < count; ++index)
        {
            length += fragments[index].length;
            pieces[index] = {fragments[index].buffer, fragments[index].bufferStart, length};
        }
    }
    explicit State(std::span<const Pointer> children) : entries(Branches{}), count(children.size()), height(children.front()->height + 1u)
    {
        auto& branches = std::get<Branches>(entries);
        for (size_t index = 0u; index < count; ++index)
        {
            length += children[index]->length;
            branches[index] = {children[index], length};
        }
    }
    size_t End(size_t index) const noexcept
    {
        return height ? std::get<Branches>(entries)[index].sourceEnd : std::get<Leaves>(entries)[index].sourceEnd;
    }
    size_t Start(size_t index) const noexcept { return index ? End(index - 1u) : 0u; }
    size_t Find(size_t offset) const noexcept
    {
        size_t first = 0u, last = count;
        while (first < last)
        {
            const auto middle = first + (last - first) / 2u;
            if (End(middle) <= offset)
                first = middle + 1u;
            else
                last = middle;
        }
        return first;
    }
    Fragment ReadPiece(size_t index) const noexcept
    {
        const auto& piece = std::get<Leaves>(entries)[index];
        return {piece.buffer, piece.bufferStart, piece.sourceEnd - Start(index)};
    }
    static void Append(std::array<Fragment, Capacity * 2u + 2u>& fragments, size_t& used, Fragment fragment)
    {
        if (!fragment.length)
            return;
        if (used && fragments[used - 1u].buffer == fragment.buffer &&
            fragments[used - 1u].bufferStart + fragments[used - 1u].length == fragment.bufferStart)
            fragments[used - 1u].length += fragment.length;
        else
            fragments[used++] = std::move(fragment);
    }
    static Result MakeLeaves(std::span<const Fragment> fragments)
    {
        if (fragments.empty())
            return {};
        if (fragments.size() <= Capacity)
            return {std::make_shared<const State>(fragments), {}};
        const auto split = fragments.size() / 2u;
        return {std::make_shared<const State>(fragments.first(split)), std::make_shared<const State>(fragments.subspan(split))};
    }
    static Result JoinSiblings(const Pointer& left, const Pointer& right)
    {
        if (!left->height)
        {
            std::array<Fragment, Capacity * 2u + 2u> fragments;
            size_t used = 0u;
            for (const auto* node : {left.get(), right.get()})
                for (size_t index = 0u; index < node->count; ++index)
                    Append(fragments, used, node->ReadPiece(index));
            return MakeLeaves({fragments.data(), used});
        }
        std::array<Pointer, Capacity * 2u + 2u> children;
        size_t used = 0u;
        for (const auto* node : {left.get(), right.get()})
            for (size_t index = 0u; index < node->count; ++index)
                children[used++] = std::get<Branches>(node->entries)[index].node;
        // A boundary split can leave a one-child chain. Normalize the lower
        // level while merging its parents, rather than retaining an underfull
        // descendant inside an otherwise full new branch.
        return MakeBranches(children, used);
    }
    static Result MakeBranches(std::array<Pointer, Capacity * 2u + 2u>& children, size_t used)
    {
        for (size_t index = 0u; used > 1u && index < used;)
        {
            if (children[index]->count >= Minimum)
            {
                ++index;
                continue;
            }
            const auto left = index ? index - 1u : 0u;
            const auto merged = JoinSiblings(children[left], children[left + 1u]);
            children[left] = merged.first;
            if (merged.second)
                children[left + 1u] = merged.second;
            else
            {
                for (size_t move = left + 1u; move + 1u < used; ++move)
                    children[move] = std::move(children[move + 1u]);
                children[--used].reset();
            }
            index = left;
        }
        if (!used)
            return {};
        if (used <= Capacity)
            return {std::make_shared<const State>(std::span<const Pointer>(children.data(), used)), {}};
        const auto split = used / 2u;
        return {std::make_shared<const State>(std::span<const Pointer>(children.data(), split)),
                std::make_shared<const State>(std::span<const Pointer>(children.data() + split, used - split))};
    }
    static Result Splice(const Pointer& node, size_t first, size_t last, const Fragment& insertion)
    {
        if (!node)
            return insertion.length ? MakeLeaves(std::span<const Fragment>(&insertion, 1u)) : Result{};
        if (!node->height)
        {
            std::array<Fragment, Capacity * 2u + 2u> fragments;
            size_t used = 0u;
            for (size_t index = 0u; index < node->count; ++index)
            {
                const auto begin = node->Start(index), end = node->End(index);
                if (begin >= first)
                    break;
                auto piece = node->ReadPiece(index);
                piece.length = std::min(first, end) - begin;
                Append(fragments, used, std::move(piece));
            }
            Append(fragments, used, insertion);
            for (auto index = node->Find(last); index < node->count; ++index)
            {
                const auto begin = node->Start(index), end = node->End(index);
                const auto offset = std::max(last, begin) - begin;
                auto piece = node->ReadPiece(index);
                piece.bufferStart += offset;
                piece.length = end - begin - offset;
                Append(fragments, used, std::move(piece));
            }
            return MakeLeaves({fragments.data(), used});
        }
        std::array<Pointer, Capacity * 2u + 2u> children;
        size_t used = 0u;
        const auto anchor = first == node->length ? node->count - 1u : node->Find(first);
        const auto& branches = std::get<Branches>(node->entries);
        for (size_t index = 0u; index < node->count; ++index)
        {
            const auto begin = node->Start(index), end = node->End(index);
            if (index != anchor && (end <= first || begin >= last))
                children[used++] = branches[index].node;
            else if (index == anchor || first > begin || last < end)
            {
                const auto relativeFirst = first > begin ? std::min(first - begin, end - begin) : 0u;
                const auto relativeLast = last > begin ? std::min(last - begin, end - begin) : 0u;
                const auto replacement = Splice(branches[index].node, relativeFirst, relativeLast, index == anchor ? insertion : Fragment{});
                if (replacement.first)
                    children[used++] = replacement.first;
                if (replacement.second)
                    children[used++] = replacement.second;
            }
            // Fully removed middle subtrees need no traversal or new nodes.
        }
        return MakeBranches(children, used);
    }
    static Pointer Replace(const Pointer& root, size_t first, size_t last, const Fragment& insertion)
    {
        auto result = Splice(root, first, last, insertion);
        Pointer next = result.first;
        if (result.second)
        {
            const std::array<Pointer, 2u> children{result.first, result.second};
            next = std::make_shared<const State>(std::span<const Pointer>(children));
        }
        while (next && next->height && next->count == 1u)
            next = std::get<Branches>(next->entries)[0].node;
        return next;
    }
    template <typename Visitor>
    static bool Visit(const State* node, size_t first, size_t last, Visitor&& visitor)
    {
        if (first == last)
            return true;
        for (auto index = node->Find(first); index < node->count; ++index)
        {
            const auto begin = node->Start(index), end = node->End(index);
            const auto left = std::max(first, begin), right = std::min(last, end);
            if (node->height)
            {
                if (!Visit(std::get<Branches>(node->entries)[index].node.get(), left - begin, right - begin, visitor))
                    return false;
            }
            else
            {
                const auto& piece = std::get<Leaves>(node->entries)[index];
                if (!visitor(std::wstring_view(*piece.buffer).substr(piece.bufferStart + left - begin, right - left)))
                    return false;
            }
            if (end >= last)
                break;
        }
        return true;
    }
};
