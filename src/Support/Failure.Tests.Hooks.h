#pragma once
#include <windows.h>
namespace DxUi::Failure::Detail
{
// Private deterministic child-test seam; event remains borrowed until process exit.
void SetFatalContenderEventForTest(HANDLE event) noexcept;
} // namespace DxUi::Failure::Detail
