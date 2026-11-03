// Common paired-performance translation unit. It deliberately includes only the
// measured fixture and its instrument, so an older library is not asked to
// compile the candidate's functional tests or test-support APIs.
#pragma warning(push)
#pragma warning(disable : 4464)
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#pragma warning(pop)
#include "Embedded.Tests.BenchmarkInstrument.h"
#include "Support/Diagnostics.h"
#include <DxUi/Embedded.h>
#include <UIAutomation.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <wil/resource.h>

static void Check(bool value, const char* text)
{
    if (! value)
    {
        std::fprintf(stderr, "Benchmark failure: %s\n", text);
        std::exit(1);
    }
}
static void Hr(HRESULT result, const char* text)
{
    if (FAILED(result))
        std::cerr << "HRESULT: " << std::hex << result << std::dec << '\n';
    Check(SUCCEEDED(result), text);
}
#include "Embedded.Tests.ComplexUiBenchmark.h"

// Each side compiles this same adapter against its own library. API 5 needs
// its real application-owned host; older libraries have no such dependency.
#if defined(DXUI_HAS_FAILURE_POLICY)
#pragma warning(push)
#pragma warning(disable : 4464)
#include "../../Samples/Shared/FailureHost.cpp"
#pragma warning(pop)
namespace DxUiFailureTests
{
inline DxUiSample::FailureHost::Host host;
inline int RunFailureChild(int, wchar_t**) noexcept
{
    return -1;
}
inline HRESULT ConfigureFailureEvidence() noexcept
{
    return S_OK;
}
inline HRESULT InstallFailureHost() noexcept
{
    return host.StartBesideExecutable();
}
inline HRESULT UninstallFailureHost() noexcept
{
    // This benchmark owns its process; all measured views have already retired.
    const auto disconnected = DxUi::Failure::InvokeNative(UiaDisconnectAllProviders);
    if (FAILED(disconnected))
        return disconnected;
    return host.UninstallWhenIdle(5000u, +[]() noexcept { return DxUi::Failure::InvokeNative(UiaDisconnectAllProviders); });
}
} // namespace DxUiFailureTests
#else
namespace DxUiFailureTests
{
inline int RunFailureChild(int, wchar_t**) noexcept
{
    return -1;
}
inline HRESULT ConfigureFailureEvidence() noexcept
{
    return S_OK;
}
inline HRESULT InstallFailureHost() noexcept
{
    return S_OK;
}
inline HRESULT UninstallFailureHost() noexcept
{
    return S_OK;
}
} // namespace DxUiFailureTests
#endif

// The paired executable accepts benchmark modes only.
static int RunFunctionalTests() noexcept
{
    return 1;
}
#include "Embedded.Tests.BenchmarkMain.h"
