// Minimal benchmark entry; this identical driver is included in the workload fingerprint.
int wmain(int argc, wchar_t** argv)
{
    InstallNativeControlAllocationInstrument();
    if (const auto child = DxUiFailureTests::RunFailureChild(argc, argv); child >= 0)
        return child;
    if (FAILED(DxUiFailureTests::ConfigureFailureEvidence()) || FAILED(DxUiFailureTests::InstallFailureHost()))
        return 1;
    Hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "COM apartment");
    const auto com         = wil::scope_exit([] { CoUninitialize(); });
    const auto failureHost = wil::scope_exit([]
    {
        const HRESULT result = DxUiFailureTests::UninstallFailureHost();
        if (FAILED(result))
        {
#if defined(DXUI_HAS_FAILURE_POLICY)
            DxUi::Failure::FailContract(result);
#else
            Check(false, "retire the process-owned failure host");
#endif
        }
    });
    if (argc == 3 && std::wstring_view(argv[1]) == L"--benchmark")
    {
#if defined(DXUI_HAS_FAILURE_POLICY)
        const DxUi::Failure::Scope failureScope;
#endif
        ComplexUiBenchmark::Run(argv[2]);
        return 0;
    }
    if (argc == 3 && std::wstring_view(argv[1]) == L"--benchmark-multiline-grid")
    {
#if defined(DXUI_HAS_FAILURE_POLICY)
        const DxUi::Failure::Scope failureScope;
#endif
        ComplexUiBenchmark::Run(argv[2], true);
        return 0;
    }
    if (argc == 3 && std::wstring_view(argv[1]) == L"--benchmark-multiline-grid-distinct")
    {
#if defined(DXUI_HAS_FAILURE_POLICY)
        const DxUi::Failure::Scope failureScope;
#endif
        ComplexUiBenchmark::Run(argv[2], true, false, false, false, true);
        return 0;
    }
    if (argc == 3 && std::wstring_view(argv[1]) == L"--benchmark-multiline-grid-retention")
    {
#if defined(DXUI_HAS_FAILURE_POLICY)
        const DxUi::Failure::Scope failureScope;
#endif
        ComplexUiBenchmark::Run(argv[2], true, true);
        return 0;
    }
    if (argc == 3 && std::wstring_view(argv[1]) == L"--benchmark-multiline-grid-heap")
    {
#if defined(DXUI_HAS_FAILURE_POLICY)
        const DxUi::Failure::Scope failureScope;
#endif
        ComplexUiBenchmark::Run(argv[2], true, true, true);
        return 0;
    }
    if (argc == 3 && std::wstring_view(argv[1]) == L"--benchmark-multiline-grid-heap-paced")
    {
#if defined(DXUI_HAS_FAILURE_POLICY)
        const DxUi::Failure::Scope failureScope;
#endif
        ComplexUiBenchmark::Run(argv[2], true, true, true, true);
        return 0;
    }
    if (argc == 3 && std::wstring_view(argv[1]) == L"--benchmark-retention")
    {
#if defined(DXUI_HAS_FAILURE_POLICY)
        const DxUi::Failure::Scope failureScope;
#endif
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
