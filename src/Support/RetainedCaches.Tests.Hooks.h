#pragma once
namespace DxUi::Detail
{
// Private deterministic test seam for the incremental-versus-fresh oracle's negative control: while enabled on the calling
// thread, Label::SetText keeps the label's retained layout current, so an incremental frame paints the old text and a
// fresh one (after DebugDropRetainedStateForTest) the new text. Declared only here, never in include/DxUi.
void SetStaleLabelLayoutInjectionForTest(bool enabled) noexcept;
// While enabled on the calling thread, preparation that would reuse a previous publication (block-incremental semantic
// projection) builds from scratch: the oracle's fresh scene sets it around its own steps and captures.
void SetFreshPreparationForTest(bool enabled) noexcept;
} // namespace DxUi::Detail
