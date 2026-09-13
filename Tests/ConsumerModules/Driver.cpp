#include <objbase.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <windows.h>

#include <cstdio>
#include <thread>

extern "C" int DxUiModuleProbe() noexcept;

int wmain()
{
    wil::unique_hmodule first(LoadLibraryW(L"NativeModuleA.dll"));
    wil::unique_hmodule second(LoadLibraryW(L"NativeModuleB.dll"));
    if (! first || ! second)
        return 10;
    using Probe            = int (*)() noexcept;
    const auto firstProbe  = reinterpret_cast<Probe>(GetProcAddress(first.get(), "DxUiModuleProbe"));
    const auto secondProbe = reinterpret_cast<Probe>(GetProcAddress(second.get(), "DxUiModuleProbe"));
    if (! firstProbe || ! secondProbe)
        return 11;
    int failures = 0;
    // Join before unloading: all module-owned UI-thread resources must finish before DLL teardown.
    for (int generation = 0; generation != 2; ++generation)
    {
        std::jthread ui([&]()
        {
            const auto com = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
            for (int round = 0; round != 2; ++round)
            {
                failures += DxUiModuleProbe() != 0;
                failures += firstProbe() != 0;
                failures += secondProbe() != 0;
            }
        });
        ui.join();
    }
    std::printf("Native module fixture: %d failed probes across EXE + two DLLs\n", failures);
    return failures == 0 ? 0 : 12;
}
