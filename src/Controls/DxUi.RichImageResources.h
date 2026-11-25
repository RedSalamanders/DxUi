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
    RichImageResources() : _entries(0u), _bindings(0u)
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
        return ! _next && ! _policyChanged && ! _suspended && _document == document && _location == location && _binding == binding;
    }
    bool CurrentForCompletion(const std::shared_ptr<const SemanticDocument>& document, std::uint64_t location, std::uint64_t binding) const noexcept
    {
        // A privacy-only rebuild retains local jobs already held by the caller.
        // Its current local completion must remain admissible before/while the
        // next bounded preparation turn republishes image placements.
        return _document == document && _location == location && _binding == binding;
    }
    bool RemoteImagesEnabled() const noexcept
    {
        return _remoteEnabled;
    }
    void SetRemoteImagesEnabled(bool enabled)
    {
        if (_remoteEnabled == enabled)
            return;
        if (! enabled)
            RetireMatching([](const Entry& entry)
            { return entry.request->sourceKind == RichImageSourceKind::RemoteHttp || entry.request->sourceKind == RichImageSourceKind::NetworkFile; });
        // Staged jobs were not exposed to the application. Their ownership list
        // excludes retained published local jobs, so this retires remote work
        // without canceling a local decode already held by the application.
        _next.reset();
        for (auto& entry : _entries)
            entry.failed = false;
        _remoteEnabled = enabled;
        _policyChanged = true;
    }
    bool DataImagesEnabled() const noexcept
    {
        return _dataEnabled;
    }
    void SetDataImagesEnabled(bool enabled)
    {
        if (_dataEnabled == enabled)
            return;
        if (! enabled)
            RetireMatching([](const Entry& entry) { return entry.request->sourceKind == RichImageSourceKind::Data; });
        _next.reset();
        for (auto& entry : _entries)
            entry.failed = false;
        _dataEnabled   = enabled;
        _policyChanged = true;
    }
    bool SvgImagesEnabled() const noexcept
    {
        return _svgEnabled;
    }
    void SetSvgImagesEnabled(bool enabled)
    {
        if (_svgEnabled == enabled)
            return;
        if (! enabled)
            RetireMatching([](const Entry& entry) { return entry.image && ! entry.image->SvgBytes().empty(); });
        _next.reset();
        for (auto& entry : _entries)
            entry.failed = false;
        _svgEnabled    = enabled;
        _policyChanged = true;
    }
    // Retain only jobs with at least one source occurrence outside the tracked
    // edit. Their results remain private until new semantics confirms ownership.
    void SuspendForEdit(const SourceSnapshot& current, const SourceSnapshot& previous) noexcept
    {
        _next.reset();
        if (! _document)
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
        const auto elements = _document->Elements();
        for (std::size_t index = 0u; index < _entries.size(); ++index)
        {
            auto& entry = _entries[index];
            if (! entry.pending)
                continue;
            const bool unaffected = std::ranges::any_of(_bindings,
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
        _suspended = true;
    }
    bool CanCompleteSuspended(std::uint64_t location, std::uint64_t binding) const noexcept
    {
        return _suspended && _document && _location == location && _binding == binding;
    }
    void Cancel() noexcept
    {
        _next.reset();
        for (auto& entry : _entries)
            if (entry.pending)
                entry.request->cancelled.store(true, std::memory_order_relaxed);
        _document.reset();
        _suspended = false;
    }
    void Clear() noexcept
    {
        Cancel();
        _entries.clear();
        _bindings.clear();
        _snapshot.reset();
        _decodedBytes  = 0u;
        _policyChanged = false;
        _snapshotDirty = false;
    }
    bool Begin(std::shared_ptr<const SemanticDocument> document, std::wstring_view base, std::uint64_t location, std::uint64_t binding)
    {
        if (! document)
            Failure::FailContract(E_INVALIDARG);
        if (Current(document, location, binding))
            return true;
        if (_next && _next->document == document && _next->location == location && _next->binding == binding)
            return false;
        if (SemanticExtensionAccess::Images(*document).empty())
        {
            Clear();
            _document = std::move(document);
            _location = location;
            _binding  = binding;
            return true;
        }
        const bool preserveLocal = (_suspended || (_policyChanged && _document == document)) && _location == location && _binding == binding;
        auto staging             = std::make_unique<Stage>(std::move(document), base, location, binding, _remoteEnabled, _dataEnabled);
        if (preserveLocal)
            _next.reset();
        else
            Cancel();
        _next.swap(staging);
        return false;
    }
    bool Advance(std::size_t* workUnits = nullptr)
    try
    {
        Failure::Scope failureScope;
        if (workUnits)
            *workUnits = 0u;
        if (! _next)
            return true;
        auto& stage       = *_next;
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
                if (stage.document->Elements()[element].wiki)
                {
                    kind = RichImageSourceKind::WikiTarget;
                    return std::optional<std::wstring>(std::in_place, destination);
                }
                const auto local = ResolveLocalImagePath(stage.base, destination);
                // A direct fallible copy avoids Debug STL proxy allocation
                // inside optional<wstring>'s noexcept move constructor.
                if (local)
                {
                    if (IsNetworkImagePath(destination, local.value()))
                    {
                        if (! stage.remoteEnabled)
                            return std::optional<std::wstring>{};
                        kind = RichImageSourceKind::NetworkFile;
                    }
                    return std::optional<std::wstring>(std::in_place, local.value());
                }
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
            }();
            if (! path)
                continue;
            const auto sameSource = [&](const auto& entry)
            {
                return entry.request->sourceKind == kind && ((kind == RichImageSourceKind::LocalFile || kind == RichImageSourceKind::NetworkFile)
                                                                 ? SameLocalImagePath(entry.request->filePath, *path)
                                                                 : entry.request->filePath == *path);
            };
            auto found = std::ranges::find_if(stage.entries, sameSource);
            if (found == stage.entries.end())
            {
                if (stage.entries.size() == MaximumSources)
                    continue;
                const auto held = std::ranges::find_if(_entries,
                                                       [&](const auto& entry)
                {
                    return (entry.image || (entry.failed && _location == stage.location && _binding == stage.binding) ||
                            (entry.pending && ! entry.request->cancelled.load(std::memory_order_relaxed))) &&
                           sameSource(entry);
                });
                if (held != _entries.end())
                    stage.entries.push_back(*held);
                else
                {
                    if (_nextId == std::numeric_limits<std::uint64_t>::max())
                        Failure::FailContract(E_BOUNDS);
                    auto request = std::make_shared<RichImageRequestState>(*path, ++_nextId, kind);
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
        _entries.swap(stage.entries);
        _bindings.swap(stage.bindings);
        _document      = stage.document;
        _location      = stage.location;
        _binding       = stage.binding;
        _snapshot      = std::move(snapshot);
        _snapshotDirty = false;
        UpdateDecodedBytes();
        _policyChanged  = false;
        _suspended      = false;
        stage.committed = true;
        _next.reset();
        return true;
    }
    catch (const std::bad_alloc&)
    {
        Failure::FailAllocation();
    }
    catch (const std::exception&)
    {
        Failure::FailUnexpected();
    }
    void Take(std::vector<RichImageRequest>& requests)
    {
        if (! _document || _next || std::none_of(_entries.begin(), _entries.end(), [](const auto& entry) { return entry.pending && ! entry.dispatched; }))
        {
            requests.clear();
            return;
        }
        std::vector<RichImageRequest> staged(0u);
        if (_document && ! _next)
        {
            staged.reserve(_entries.size());
            for (const auto& entry : _entries)
                if (entry.pending && ! entry.dispatched)
                    staged.push_back(RichImageRequest(entry.request));
        }
        requests.swap(staged);
        // No fallible work follows publication of the caller-owned list.
        for (auto& entry : _entries)
            if (entry.pending)
                entry.dispatched = true;
    }
    bool Complete(const RichImageRequest& request, std::shared_ptr<const RichImage> image)
    {
        if (! _document || ! request._state || request.Cancelled() || (image && ! image->SvgBytes().empty() && ! _svgEnabled))
            return false;
        const auto found = std::ranges::find(_entries, request._state, &Entry::request);
        if (found == _entries.end() || ! found->pending || ! found->dispatched)
            return false;
        const auto additional =
            image && std::none_of(_entries.begin(), _entries.end(), [&](const auto& entry) { return entry.image == image; }) ? image->RetainedBytes() : 0u;
        const bool fits = image && additional <= MaximumDecodedBytes - _decodedBytes;
        found->pending  = false;

        found->failed = ! fits;
        found->request->cancelled.store(true, std::memory_order_relaxed);
        if (fits)
        {
            _decodedBytes += additional;
            found->image   = std::move(image);
            _snapshotDirty = true;
        }
        if (_next)
        {
            const auto staged = std::ranges::find(_next->entries, found->request, &Entry::request);
            if (staged != _next->entries.end())
                *staged = *found;
        }
        return true;
    }
    bool HasUnpublished() const noexcept
    {
        return _snapshotDirty;
    }
    bool ReplaceFrame(const RichImageRequest& request, const std::shared_ptr<const RichImage>& image) noexcept
    {
        if (! _document || _next || ! request._state || ! image || (! image->SvgBytes().empty() && ! _svgEnabled))
            return false;
        const auto found = std::ranges::find(_entries, request._state, &Entry::request);
        if (found == _entries.end() || found->pending || ! found->image)
            return false;
        std::size_t bytes{};
        for (auto entry = _entries.begin(); entry != _entries.end(); ++entry)
        {
            const auto& pixels = entry == found ? image : entry->image;
            if (! pixels)
                continue;
            bool duplicate = false;
            for (auto prior = _entries.begin(); prior != entry; ++prior)
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
            _decodedBytes  = bytes;
            _snapshotDirty = true;
        }
        return true;
    }
    void Publish()
    {
        if (! _snapshotDirty)
            return;
        auto next      = MakeSnapshot(_entries, _bindings);
        _snapshot      = std::move(next);
        _snapshotDirty = false;
    }
    std::shared_ptr<const RichImageResourceSet> Snapshot() const noexcept
    {
        return _snapshot;
    }
    bool Pending() const noexcept
    {
        return static_cast<bool>(_next);
    }
    RichImageResourceStatistics Statistics() const noexcept
    {
        RichImageResourceStatistics result{.imageElements = _bindings.size(), .requestedSources = _entries.size(), .decodedBytes = _decodedBytes};
        for (const auto& entry : _entries)
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
        retained.reserve(_entries.size());
        bindings.reserve(_bindings.size());
        for (const auto& binding : _bindings)
        {
            const auto& entry = _entries[binding.entry];
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
        _entries.swap(retained);
        _bindings.swap(bindings);
        _snapshot      = std::move(snapshot);
        _snapshotDirty = false;
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
        for (auto& entry : _entries)
            if (entry.pending)
                entry.request->cancelled.store(true, std::memory_order_relaxed);
    }
    void CancelPublishedExcept(const std::vector<Entry>& retained) noexcept
    {
        for (auto& entry : _entries)
            if (entry.pending && std::ranges::find(retained, entry.request, &Entry::request) == retained.end())
                entry.request->cancelled.store(true, std::memory_order_relaxed);
    }
    void UpdateDecodedBytes() noexcept
    {
        _decodedBytes = 0u;
        for (std::size_t index = 0u; index < _entries.size(); ++index)
            if (_entries[index].image && std::none_of(_entries.begin(), _entries.begin() + static_cast<std::ptrdiff_t>(index), [&](const auto& prior) {
                return prior.image == _entries[index].image;
            }))
                _decodedBytes += _entries[index].image->RetainedBytes();
    }
    std::vector<Entry> _entries;
    std::vector<Binding> _bindings;
    std::shared_ptr<const SemanticDocument> _document;
    std::shared_ptr<const RichImageResourceSet> _snapshot;
    std::unique_ptr<Stage> _next;
    std::uint64_t _location{}, _binding{}, _nextId{};
    std::size_t _decodedBytes{};
    bool _remoteEnabled{}, _dataEnabled{}, _svgEnabled{}, _policyChanged{}, _snapshotDirty{}, _suspended{};
};
} // namespace DxUi::RichText::Detail
