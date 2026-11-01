#pragma once
#include "DxUi.RichImage.h"
#include "DxUi.RichImagePaths.h"
#include "DxUi.SemanticExtensions.h"
#include <limits>

namespace DxUi::RichText::Detail
{
struct RichImagePlacementResource
{
    std::size_t element{};
    std::shared_ptr<const RichImage> image;
};
struct RichImageResourceSet
{
    RichImageResourceSet() : placements(0u)
    {
    }
    std::vector<RichImagePlacementResource> placements;
};
// Owner-thread resource state only. Jobs execute in an application service;
// cancellation is an atomic store, with no callbacks, waits or worker ownership.
class RichImageResources
{
public:
    static constexpr std::size_t MaximumSources = 32u, MaximumPlacements = 128u, MaximumDecodedBytes = 67108864u;
    RichImageResources() : entries_(0u), bindings_(0u)
    {
    }
    ~RichImageResources()
    {
        Cancel();
    }
    RichImageResources(const RichImageResources&)            = delete;
    RichImageResources& operator=(const RichImageResources&) = delete;
    bool Current(const std::shared_ptr<const SemanticDocument>& document, std::uint64_t location, std::uint64_t binding) const noexcept
    {
        return ! next_ && ! policyChanged_ && ! suspended_ && document_ == document && location_ == location && binding_ == binding;
    }
    bool CurrentForCompletion(const std::shared_ptr<const SemanticDocument>& document, std::uint64_t location, std::uint64_t binding) const noexcept
    {
        // A privacy-only rebuild retains local jobs already held by the caller.
        // Its current local completion must remain admissible before/while the
        // next bounded preparation turn republishes image placements.
        return document_ == document && location_ == location && binding_ == binding;
    }
    bool RemoteImagesEnabled() const noexcept
    {
        return remoteEnabled_;
    }
    void SetRemoteImagesEnabled(bool enabled)
    {
        if (remoteEnabled_ == enabled)
            return;
        if (! enabled)
            RetireMatching([](const Entry& entry) { return entry.request->sourceKind == RichImageSourceKind::RemoteHttp; });
        // Staged jobs were not exposed to the application. Their ownership list
        // excludes retained published local jobs, so this retires remote work
        // without canceling a local decode already held by the application.
        next_.reset();
        for (auto& entry : entries_)
            entry.failed = false;
        remoteEnabled_ = enabled;
        policyChanged_ = true;
    }
    bool DataImagesEnabled() const noexcept
    {
        return dataEnabled_;
    }
    void SetDataImagesEnabled(bool enabled)
    {
        if (dataEnabled_ == enabled)
            return;
        if (! enabled)
            RetireMatching([](const Entry& entry) { return entry.request->sourceKind == RichImageSourceKind::Data; });
        next_.reset();
        for (auto& entry : entries_)
            entry.failed = false;
        dataEnabled_   = enabled;
        policyChanged_ = true;
    }
    bool SvgImagesEnabled() const noexcept
    {
        return svgEnabled_;
    }
    void SetSvgImagesEnabled(bool enabled)
    {
        if (svgEnabled_ == enabled)
            return;
        if (! enabled)
            RetireMatching([](const Entry& entry) { return entry.image && ! entry.image->SvgBytes().empty(); });
        next_.reset();
        for (auto& entry : entries_)
            entry.failed = false;
        svgEnabled_    = enabled;
        policyChanged_ = true;
    }
    // Retain only jobs with at least one source occurrence outside the tracked
    // edit. Their results remain private until new semantics confirms ownership.
    void SuspendForEdit(const SourceSnapshot& current, const SourceSnapshot& previous) noexcept
    {
        next_.reset();
        if (! document_)
        {
            Cancel();
            return;
        }
        const auto change = current.ChangedRangeFrom(previous);
        if (change.kind == SourceChangeKind::FullReplacement)
        {
            Cancel();
            return;
        }
        const auto elements = document_->Elements();
        for (std::size_t index = 0u; index < entries_.size(); ++index)
        {
            auto& entry = entries_[index];
            if (! entry.pending)
                continue;
            const bool unaffected = std::ranges::any_of(bindings_,
                                                        [&](const auto& binding)
            {
                if (binding.entry != index || binding.element >= elements.size())
                    return false;
                const auto range = elements[binding.element].source;
                return range.last <= change.begin || range.first >= change.previousEnd;
            });
            if (! unaffected)
                entry.request->cancelled.store(true, std::memory_order_relaxed);
        }
        suspended_ = true;
    }
    bool CanCompleteSuspended(std::uint64_t location, std::uint64_t binding) const noexcept
    {
        return suspended_ && document_ && location_ == location && binding_ == binding;
    }
    void Cancel() noexcept
    {
        next_.reset();
        for (auto& entry : entries_)
            if (entry.pending)
                entry.request->cancelled.store(true, std::memory_order_relaxed);
        document_.reset();
        suspended_ = false;
    }
    void Clear() noexcept
    {
        Cancel();
        entries_.clear();
        bindings_.clear();
        snapshot_.reset();
        decodedBytes_  = 0u;
        policyChanged_ = false;
        snapshotDirty_ = false;
    }
    bool Begin(std::shared_ptr<const SemanticDocument> document, std::wstring_view base, std::uint64_t location, std::uint64_t binding)
    {
        if (! document)
            throw std::invalid_argument("image preparation needs a current semantic document");
        if (Current(document, location, binding))
            return true;
        if (next_ && next_->document == document && next_->location == location && next_->binding == binding)
            return false;
        if (SemanticExtensionAccess::Images(*document).empty())
        {
            Clear();
            document_ = std::move(document);
            location_ = location;
            binding_  = binding;
            return true;
        }
        const bool preserveLocal = (suspended_ || (policyChanged_ && document_ == document)) && location_ == location && binding_ == binding;
        auto staging             = std::make_unique<Stage>(std::move(document), base, location, binding, remoteEnabled_, dataEnabled_);
        if (preserveLocal)
            next_.reset();
        else
            Cancel();
        next_.swap(staging);
        return false;
    }
    bool Advance(std::size_t* workUnits = nullptr)
    try
    {
        if (workUnits)
            *workUnits = 0u;
        if (! next_)
            return true;
        auto& stage       = *next_;
        const auto images = SemanticExtensionAccess::Images(*stage.document);
        // Match the existing native-object ceiling with diagrams already
        // admitted by the semantic builder; excess images retain their alt.
        const auto diagrams = SemanticExtensionAccess::Diagrams(*stage.document).size() + SemanticExtensionAccess::Maths(*stage.document).size();
        const auto count    = std::min(images.size(), MaximumPlacements - std::min(MaximumPlacements, diagrams));
        for (std::size_t work = 0u; stage.cursor < count && work < 4u; ++work, ++stage.cursor)
        {
            if (workUnits)
                ++*workUnits;
            const auto element       = images[stage.cursor];
            const auto& destination  = stage.document->Elements()[element].destination;
            RichImageSourceKind kind = RichImageSourceKind::LocalFile;
            const auto path          = [&]
            {
                try
                {
                    if (stage.document->Elements()[element].wiki)
                    {
                        kind = RichImageSourceKind::WikiTarget;
                        return std::optional<std::wstring>(std::in_place, destination);
                    }
                    const auto local = ResolveLocalImagePath(stage.base, destination);
                    // A direct fallible copy avoids Debug STL proxy allocation
                    // inside optional<wstring>'s noexcept move constructor.
                    if (local)
                        return std::optional<std::wstring>(std::in_place, local.value());
                    if (stage.dataEnabled)
                        if (const auto data = ResolveDataImageSource(destination))
                        {
                            kind = RichImageSourceKind::Data;
                            return std::optional<std::wstring>(std::in_place, data.value());
                        }
                    if (! stage.remoteEnabled)
                        return std::optional<std::wstring>{};
                    kind = RichImageSourceKind::RemoteHttp;
                    return ResolveRemoteImageSource(destination);
                }
                catch (const std::length_error&)
                {
                    return std::optional<std::wstring>{};
                }
            }();
            if (! path)
                continue;
            const auto sameSource = [&](const auto& entry)
            {
                return entry.request->sourceKind == kind &&
                       (kind == RichImageSourceKind::LocalFile ? SameLocalImagePath(entry.request->filePath, *path) : entry.request->filePath == *path);
            };
            auto found = std::ranges::find_if(stage.entries, sameSource);
            if (found == stage.entries.end())
            {
                if (stage.entries.size() == MaximumSources)
                    continue;
                const auto held = std::ranges::find_if(entries_,
                                                       [&](const auto& entry)
                {
                    return (entry.image || (entry.failed && location_ == stage.location && binding_ == stage.binding) ||
                            (entry.pending && ! entry.request->cancelled.load(std::memory_order_relaxed))) &&
                           sameSource(entry);
                });
                if (held != entries_.end())
                    stage.entries.push_back(*held);
                else
                {
                    if (nextId_ == std::numeric_limits<std::uint64_t>::max())
                        throw std::overflow_error("image request identity exhausted");
                    auto request = std::make_shared<RichImageRequestState>(*path, ++nextId_, kind);
                    stage.introduced.push_back(request);
                    stage.entries.push_back({std::move(request), {}, true, false, false});
                }
                found = std::prev(stage.entries.end());
            }
            stage.bindings.push_back({element, static_cast<std::size_t>(found - stage.entries.begin())});
        }
        if (stage.cursor < count)
            return false;
        auto snapshot = MakeSnapshot(stage.entries, stage.bindings);
        CancelPublishedExcept(stage.entries);
        entries_.swap(stage.entries);
        bindings_.swap(stage.bindings);
        document_      = stage.document;
        location_      = stage.location;
        binding_       = stage.binding;
        snapshot_      = std::move(snapshot);
        snapshotDirty_ = false;
        UpdateDecodedBytes();
        policyChanged_  = false;
        suspended_      = false;
        stage.committed = true;
        next_.reset();
        return true;
    }
    catch (const std::exception&)
    {
        Cancel();
        throw;
    }
    void Take(std::vector<RichImageRequest>& requests)
    {
        if (! document_ || next_ || std::none_of(entries_.begin(), entries_.end(), [](const auto& entry) { return entry.pending && ! entry.dispatched; }))
        {
            requests.clear();
            return;
        }
        std::vector<RichImageRequest> staged(0u);
        if (document_ && ! next_)
        {
            staged.reserve(entries_.size());
            for (const auto& entry : entries_)
                if (entry.pending && ! entry.dispatched)
                    staged.push_back(RichImageRequest(entry.request));
        }
        requests.swap(staged);
        // No fallible work follows publication of the caller-owned list.
        for (auto& entry : entries_)
            if (entry.pending)
                entry.dispatched = true;
    }
    bool Complete(const RichImageRequest& request, std::shared_ptr<const RichImage> image)
    {
        if (! document_ || ! request.state_ || request.Cancelled() || (image && ! image->SvgBytes().empty() && ! svgEnabled_))
            return false;
        const auto found = std::ranges::find(entries_, request.state_, &Entry::request);
        if (found == entries_.end() || ! found->pending || ! found->dispatched)
            return false;
        const auto additional =
            image && std::none_of(entries_.begin(), entries_.end(), [&](const auto& entry) { return entry.image == image; }) ? image->RetainedBytes() : 0u;
        const bool fits = image && additional <= MaximumDecodedBytes - decodedBytes_;
        found->pending  = false;

        found->failed = ! fits;
        found->request->cancelled.store(true, std::memory_order_relaxed);
        if (fits)
        {
            decodedBytes_ += additional;
            found->image   = std::move(image);
            snapshotDirty_ = true;
        }
        if (next_)
        {
            const auto staged = std::ranges::find(next_->entries, found->request, &Entry::request);
            if (staged != next_->entries.end())
                *staged = *found;
        }
        return true;
    }
    bool HasUnpublished() const noexcept
    {
        return snapshotDirty_;
    }
    bool ReplaceFrame(const RichImageRequest& request, const std::shared_ptr<const RichImage>& image) noexcept
    {
        if (! document_ || next_ || ! request.state_ || ! image || (! image->SvgBytes().empty() && ! svgEnabled_))
            return false;
        const auto found = std::ranges::find(entries_, request.state_, &Entry::request);
        if (found == entries_.end() || found->pending || ! found->image)
            return false;
        std::size_t bytes{};
        for (auto entry = entries_.begin(); entry != entries_.end(); ++entry)
        {
            const auto& pixels = entry == found ? image : entry->image;
            if (! pixels)
                continue;
            bool duplicate = false;
            for (auto prior = entries_.begin(); prior != entry; ++prior)
                if ((prior == found ? image : prior->image) == pixels)
                {
                    duplicate = true;
                    break;
                }
            if (! duplicate)
            {
                if (pixels->RetainedBytes() > MaximumDecodedBytes - bytes)
                    return false;
                bytes += pixels->RetainedBytes();
            }
        }
        if (found->image != image)
        {
            found->image   = image;
            decodedBytes_  = bytes;
            snapshotDirty_ = true;
        }
        return true;
    }
    void Publish()
    {
        if (! snapshotDirty_)
            return;
        auto next      = MakeSnapshot(entries_, bindings_);
        snapshot_      = std::move(next);
        snapshotDirty_ = false;
    }
    std::shared_ptr<const RichImageResourceSet> Snapshot() const noexcept
    {
        return snapshot_;
    }
    bool Pending() const noexcept
    {
        return static_cast<bool>(next_);
    }
    RichImageResourceStatistics Statistics() const noexcept
    {
        RichImageResourceStatistics result{.imageElements = bindings_.size(), .requestedSources = entries_.size(), .decodedBytes = decodedBytes_};
        for (const auto& entry : entries_)
        {
            if (entry.image)
                ++result.readySources;
            else if (entry.pending && ! entry.request->cancelled.load(std::memory_order_relaxed))
                ++result.pendingSources;
            else if (entry.failed)
                ++result.failedSources;
        }
        return result;
    }

private:
    struct Entry
    {
        std::shared_ptr<RichImageRequestState> request;
        std::shared_ptr<const RichImage> image;
        bool pending{}, dispatched{}, failed{};
    };
    struct Binding
    {
        std::size_t element{}, entry{};
    };
    template <typename Predicate> void RetireMatching(Predicate predicate)
    {
        std::vector<Entry> retained(0u);
        std::vector<Binding> bindings(0u);
        retained.reserve(entries_.size());
        bindings.reserve(bindings_.size());
        for (const auto& binding : bindings_)
        {
            const auto& entry = entries_[binding.entry];
            if (predicate(entry))
                continue;
            auto found = std::ranges::find(retained, entry.request, &Entry::request);
            if (found == retained.end())
            {
                retained.push_back(entry);
                found = std::prev(retained.end());
            }
            bindings.push_back({binding.element, static_cast<std::size_t>(found - retained.begin())});
        }
        auto snapshot = MakeSnapshot(retained, bindings);
        CancelPublishedExcept(retained);
        entries_.swap(retained);
        bindings_.swap(bindings);
        snapshot_      = std::move(snapshot);
        snapshotDirty_ = false;
        UpdateDecodedBytes();
    }
    struct Stage
    {
        Stage(std::shared_ptr<const SemanticDocument> value, std::wstring_view path, std::uint64_t location, std::uint64_t binding, bool remote, bool data)
            : document(std::move(value)),
              base(path),
              location(location),
              binding(binding),
              entries(0u),
              bindings(0u),
              introduced(0u),
              remoteEnabled(remote),
              dataEnabled(data)
        {
            entries.reserve(MaximumSources);
            bindings.reserve(MaximumPlacements);
        }
        ~Stage()
        {
            if (! committed)
                for (const auto& request : introduced)
                    request->cancelled.store(true, std::memory_order_relaxed);
        }
        std::shared_ptr<const SemanticDocument> document;
        std::wstring base;
        std::uint64_t location, binding;
        std::vector<Entry> entries;
        std::vector<Binding> bindings;
        std::vector<std::shared_ptr<RichImageRequestState>> introduced;
        std::size_t cursor{};
        bool remoteEnabled{}, dataEnabled{}, committed{};
    };
    static std::shared_ptr<const RichImageResourceSet> MakeSnapshot(const std::vector<Entry>& entries,
                                                                    const std::vector<Binding>& bindings,
                                                                    std::size_t replacement                       = MarkdownNoNode,
                                                                    const std::shared_ptr<const RichImage>& image = {})
    {
        auto result = std::make_shared<RichImageResourceSet>();
        result->placements.reserve(bindings.size());
        for (const auto& binding : bindings)
        {
            const auto& pixels = binding.entry == replacement ? image : entries[binding.entry].image;
            if (pixels)
                result->placements.push_back({binding.element, pixels});
        }
        return result;
    }
    void CancelPublished() noexcept
    {
        for (auto& entry : entries_)
            if (entry.pending)
                entry.request->cancelled.store(true, std::memory_order_relaxed);
    }
    void CancelPublishedExcept(const std::vector<Entry>& retained) noexcept
    {
        for (auto& entry : entries_)
            if (entry.pending && std::ranges::find(retained, entry.request, &Entry::request) == retained.end())
                entry.request->cancelled.store(true, std::memory_order_relaxed);
    }
    void UpdateDecodedBytes() noexcept
    {
        decodedBytes_ = 0u;
        for (std::size_t index = 0u; index < entries_.size(); ++index)
            if (entries_[index].image && std::none_of(entries_.begin(), entries_.begin() + static_cast<std::ptrdiff_t>(index), [&](const auto& prior) {
                return prior.image == entries_[index].image;
            }))
                decodedBytes_ += entries_[index].image->RetainedBytes();
    }
    std::vector<Entry> entries_;
    std::vector<Binding> bindings_;
    std::shared_ptr<const SemanticDocument> document_;
    std::shared_ptr<const RichImageResourceSet> snapshot_;
    std::unique_ptr<Stage> next_;
    std::uint64_t location_{}, binding_{}, nextId_{};
    std::size_t decodedBytes_{};
    bool remoteEnabled_{}, dataEnabled_{}, svgEnabled_{}, policyChanged_{}, snapshotDirty_{}, suspended_{};
};
} // namespace DxUi::RichText::Detail
