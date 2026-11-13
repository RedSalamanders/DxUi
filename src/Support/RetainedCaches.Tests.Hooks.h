#pragma once
namespace DxUi::Detail
{
// Private deterministic test seam for the incremental-versus-fresh oracle's negative control: while enabled on the calling
// thread, Label::SetText keeps the label's retained layout current, so an incremental frame paints the old text and a
// fresh one (after DebugDropRetainedStateForTest) the new text. Declared only here, never in include/DxUi.
void SetStaleLabelLayoutInjectionForTest(bool enabled) noexcept;
} // namespace DxUi::Detail
