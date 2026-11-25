#pragma once
#include <windows.h>

namespace DxUi::Detail
{
// Private deterministic test seam of native frame pacing (W3.6), declared only here, never in include/DxUi: the next Present or
// Present1 a native host on the calling thread makes reports `result` instead of what the call returned (the call itself still
// happens). It lets a test give the host a success status such as DXGI_STATUS_OCCLUDED, which flip-model chains on the
// supported baseline never return. One present only; S_OK clears it.
void SetNextPresentResultForTest(HRESULT result) noexcept;
} // namespace DxUi::Detail
