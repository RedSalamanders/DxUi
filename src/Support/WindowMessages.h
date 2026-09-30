#pragma once
#include <windows.h>
namespace DxUi::WndMsg
{
inline constexpr UINT kContextMenuRootHoverChanged = WM_APP + 0x539;
inline constexpr UINT kAccessibilityAction         = WM_APP + 0x06A;
inline constexpr UINT kAccessibilityCreateProvider = WM_APP + 0x06B;
inline constexpr UINT kWindowHostProcessExitDetach = WM_APP + 0x06C;
// Posted by a window host when its window gains focus: its arrival ends the message-loop turn of the gain.
inline constexpr UINT kWindowHostFocusGainTurnEnd = WM_APP + 0x06D;
} // namespace DxUi::WndMsg
