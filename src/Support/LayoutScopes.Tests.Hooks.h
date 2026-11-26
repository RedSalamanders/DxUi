#pragma once
#include <DxUi/DxUi.h>

namespace DxUi::Detail
{
// Private test seams for layout invalidation scopes (Specs/UI/UI_ControlsAndLayout.md, "Layout invalidation scopes", W6.3).
// Declared only here.

// Whether layout scopes are on: a preparation pass enters only the controls marked as holding work below them, and an
// assignment's measure invalidation waits for its cutoff check. On by default; a test turns both off on the calling thread to
// compare with the whole-tree walk and the eager invalidation; DXUI_LAYOUT_SCOPES=0 turns them off for a whole Debug program.
[[nodiscard]] bool LayoutScopePruningEnabled() noexcept;
void SetLayoutScopePruningForTest(bool enabled) noexcept;

// While enabled on the calling thread, a Debug build follows every completed outermost preparation pass with a whole layout:
// it records every control's bounds, drops every measure cache, asks every intrinsic owner for layout, runs the pass again and
// counts each control whose bounds differ in layout.scopeVerifyMismatches. DXUI_LAYOUT_VERIFY=1 enables it for a whole test
// program. Returns whether verification exists in this build (Debug and ASan Debug only).
bool SetLayoutScopeVerificationForTest(bool enabled) noexcept;
[[nodiscard]] bool LayoutScopeVerificationEnabled() noexcept;
// Process totals of the verification, over every thread: passes verified and controls whose bounds differed.
struct LayoutScopeVerificationTotals final
{
    uint64_t verified   = 0u;
    uint64_t mismatches = 0u;
};
[[nodiscard]] LayoutScopeVerificationTotals ReadLayoutScopeVerificationTotals() noexcept;
void NoteLayoutScopeVerification(uint64_t mismatches) noexcept;

// Negative control: while enabled, a queued measure invalidation stops at its control without being checked, as a wrong
// cutoff would. The oracle and the verification must then see a stale bound.
void SetLayoutCutoffUncheckedForTest(bool enabled) noexcept;
[[nodiscard]] bool LayoutCutoffUncheckedForTest() noexcept;
} // namespace DxUi::Detail
