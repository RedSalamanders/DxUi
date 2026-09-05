#pragma once
#include <windows.h>
namespace WndMsg
{
inline constexpr UINT kDxUiContextMenuRootHoverChanged = WM_APP + 0x539;
inline constexpr UINT kDxUiAccessibilityAction         = WM_APP + 0x06A;
inline constexpr UINT kDxUiAccessibilityCreateProvider = WM_APP + 0x06B;
inline constexpr UINT kDxUiWindowHostProcessExitDetach = WM_APP + 0x06C;
} // namespace WndMsg
