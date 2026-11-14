#pragma once
#include "DxUi.h"

#include <span>
#include <string_view>
#include <vector>

namespace DxUi
{
class EmbeddedHost;

// The retained-cache registry behind DebugDropRetainedStateForTest (Testing_Validation.md, "Incremental-versus-fresh
// oracle"). Every cache a host or a control keeps between frames, derived from model, theme, DPI, typography or device
// state and rebuilt on demand, is one row: a name, a count of its live entries and a drop that returns it to the state of
// a freshly created host or control. Count and drop come from the same row, so a cache cannot be dropped without being
// counted or counted without being dropped, and the oracle requires every count to read 0 right after the drop. Pure
// model state (text, selection, scroll offsets, focus, hover, capture, animation progress) is not a cache and is kept.
// A new cache adds its row in DxUi.RetainedCaches.cpp, or an exemption with its reason in kRetainedCacheExemptions;
// Tools/tests/Test-RetainedCacheRegistry.ps1 refuses a cache-named member that is in neither.
struct RetainedCacheCount final
{
    std::string_view name;
    size_t entries = 0u;
    // Resident bytes (W9.1): the heap storage DxUi holds for the cache, outside the owning object's own size (container
    // element storage by capacity, heap string storage, bitmap and surface pixels). The storage of DirectWrite and Direct2D
    // objects is private to those APIs and never included. false: the row has no byte count (bytes then reads 0).
    bool bytesCounted      = false;
    uint64_t residentBytes = 0u;
};

// A cache named here is deliberately kept by the drop, with the reason; the registry tooling test reads this list.
struct RetainedCacheExemption final
{
    std::string_view name;
    std::string_view reason;
};

// residentBytes, when set, reports the row's resident bytes as RetainedCacheCount defines them; a row without one is
// counted by entries only and reported as such. It reads, never allocates and never changes the cache.
struct RetainedHostCacheRow final
{
    std::string_view name;
    size_t (*entries)(const ControlHost& host) noexcept;
    void (*drop)(ControlHost& host) noexcept;
    uint64_t (*residentBytes)(const ControlHost& host) noexcept = nullptr;
};

// Each row's entries/drop pair handles its own control type and returns 0 / does nothing for every other one.
struct RetainedControlCacheRow final
{
    std::string_view name;
    size_t (*entries)(const Control& control) noexcept;
    void (*drop)(Control& control) noexcept;
    uint64_t (*residentBytes)(const Control& control) noexcept = nullptr;
};

// Registry totals over one host and its tree: what EmbeddedStatistics::retainedCacheEntries and retainedCacheBytes report.
struct RetainedCacheTotals final
{
    uint64_t entries = 0u;
    uint64_t bytes   = 0u;
};

struct RetainedCacheAccess final
{
    // Drops every registered cache of the host, its tree and its tooltip layer, then asks for a full preparation:
    // native hosts invalidate their window, the next embedded Prepare repaints. UI thread only; never during paint.
    static void Drop(ControlHost& host) noexcept;
    // Appends one count per registered cache, per-control caches summed over the tree, in registry order.
    static void Count(ControlHost& host, std::vector<RetainedCacheCount>& out);
    // Adds every host row of `host`, or every control row of `control` alone (not its children), to `totals`. For
    // EmbeddedHost::GetStatistics, which visits each control once in its own walk: no allocation, no extra tree visit.
    static void AddHostTotals(const ControlHost& host, RetainedCacheTotals& totals) noexcept;
    static void AddControlTotals(const Control& control, RetainedCacheTotals& totals) noexcept;
    // The registered names, host caches first, then per-control caches.
    [[nodiscard]] static std::span<const std::string_view> Names() noexcept;
    [[nodiscard]] static std::span<const RetainedCacheExemption> Exemptions() noexcept;
    // Defined in Embedded.cpp: the embedded surface is a cache of the view (W4.7 keeps it between preparations).
    static void Drop(EmbeddedHost& host) noexcept;
    static void Count(EmbeddedHost& host, std::vector<RetainedCacheCount>& out);

private:
    // The rows live here so their entries/drop functions share this struct's friendship with the classes they read.
    [[nodiscard]] static std::span<const RetainedHostCacheRow> HostRows() noexcept;
    [[nodiscard]] static std::span<const RetainedControlCacheRow> ControlRows() noexcept;
};

// Defined in DxUi.Accessibility.cpp: the published UI Automation snapshot of a window host or an attached embedded view.
// The drop publishes nothing in its place and leaves the publication dirty, so the next provider query (native) or
// UpdateAccessibility (embedded) builds it again from the tree.
void DropRetainedAccessibilitySnapshot(ControlHost& host) noexcept;
[[nodiscard]] size_t CountRetainedAccessibilitySnapshotRecords(const ControlHost& host) noexcept;
// The snapshot's record storage by capacity and the heap storage of its records' strings (W9.1).
[[nodiscard]] uint64_t RetainedAccessibilitySnapshotBytes(const ControlHost& host) noexcept;

// Test seam: true only while a test injects a stale Label layout (RetainedCaches.Tests.Hooks.h). Label::SetText then
// leaves its retained layout current, as a cache that forgets an invalidation would, so the oracle must see a
// difference. Nothing outside the native test executables can set it: the setter is declared in no public header.
[[nodiscard]] bool IsStaleLabelLayoutInjectedForTest() noexcept;
} // namespace DxUi
