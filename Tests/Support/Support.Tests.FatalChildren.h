#pragma once
#include <cstdint>
#include <string_view>

namespace DxUiFailureTests
{
enum class FatalFaultClass
{
    CppException,
    InjectedAllocation,
    ContractViolation,
};
struct FatalChildCase
{
    const wchar_t* name;
    const char* expectedOriginSymbol;
    void (*trigger)();
    FatalFaultClass faultClass   = FatalFaultClass::CppException;
    bool scopeTrigger            = true;
    std::uint32_t expectedResult = 0u;
};
// The name and symbol must be process-lifetime literals; register before child
// dispatch in both parent and child. The registry is bounded and allocation-free.
[[nodiscard]] bool RegisterFatalChildCase(FatalChildCase entry) noexcept;
// Called immediately before the test allocator throws its injected bad_alloc.
// This is allocation-free and does nothing outside a registered child.
void MarkInjectedAllocation() noexcept;
// Set to an existing, test-owned output directory before running parent tests.
[[nodiscard]] bool ConfigureFailureChildren(std::wstring_view outputDirectory);
// Call before ordinary suite setup. Returns -1 when arguments are not a child request.
[[nodiscard]] int RunFailureChild(int argc, char** argv);
[[nodiscard]] int RunFailureChild(int argc, wchar_t** argv);
[[nodiscard]] bool ExpectFatalChild(const wchar_t* caseName);
[[nodiscard]] bool TestFailurePolicy();
} // namespace DxUiFailureTests
