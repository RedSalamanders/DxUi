// Minimal benchmark entry; this identical driver is included in the workload fingerprint.
int wmain(int argc, wchar_t** argv)
{
    Hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "COM apartment");
    const auto com = wil::scope_exit([] { CoUninitialize(); });
    if (argc == 3 && std::wstring_view(argv[1]) == L"--benchmark")
    {
        ComplexUiBenchmark::Run(argv[2]);
        return 0;
    }
    if (argc == 3 && std::wstring_view(argv[1]) == L"--benchmark-retention")
    {
        // Fixed repeated create/render/hide/destroy cycles distinguish persistent
        // process growth from initialization pools; each raw round is retained.
        for (unsigned int cycle = 0; cycle < 60; ++cycle)
        {
            const std::wstring path = std::wstring(argv[2]) + L"-" + std::to_wstring(cycle) + L".json";
            ComplexUiBenchmark::Run(path.c_str());
        }
        return 0;
    }
    return RunFunctionalTests();
}
