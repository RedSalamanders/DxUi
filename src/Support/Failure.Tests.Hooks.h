#pragma once
#include <cstddef>
#include <windows.h>
namespace DxUi::Failure::Detail
{
// Private deterministic child-test seam; event remains borrowed until process exit.
void SetFatalContenderEventForTest(HANDLE event) noexcept;
void SetYamlNativeAllocationFailureForTest(std::size_t count) noexcept;
} // namespace DxUi::Failure::Detail
