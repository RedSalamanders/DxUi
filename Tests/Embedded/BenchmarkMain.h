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
    return RunFunctionalTests();
}
