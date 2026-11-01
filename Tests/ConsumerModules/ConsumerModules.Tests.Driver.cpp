#include <objbase.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <windows.h>

#include <bit>
#include <cstdio>
#include <initializer_list>
#include <thread>

extern "C" int DxUiModuleProbe() noexcept;

int wmain()
{
    wil::unique_hmodule first(LoadLibraryW(L"NativeModuleA.dll"));
    wil::unique_hmodule second(LoadLibraryW(L"NativeModuleB.dll"));
    if (! first || ! second)
        return 10;
    using Probe            = int (*)() noexcept;
    const auto firstProbe  = std::bit_cast<Probe>(GetProcAddress(first.get(), "DxUiModuleProbe"));
    const auto secondProbe = std::bit_cast<Probe>(GetProcAddress(second.get(), "DxUiModuleProbe"));
    if (! firstProbe || ! secondProbe)
        return 11;
    int executed = 0;
    int passed   = 0;
    // Join before unloading: all module-owned UI-thread resources must finish before DLL teardown.
    for (int generation = 0; generation != 2; ++generation)
    {
        std::jthread ui([&]()
        {
            const auto com = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
            for (int round = 0; round != 2; ++round)
            {
                for (const auto probe : {DxUiModuleProbe, firstProbe, secondProbe})
                {
                    const int result = probe();
                    ++executed;
                    if (result == 0)
                        ++passed;
                }
            }
        });
        ui.join();
    }
    std::printf("DXUI_NATIVE_MODULE_PROBES executed=%d passed=%d\n", executed, passed);
    return executed == 12 && passed == executed ? 0 : 12;
}
