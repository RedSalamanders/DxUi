#include "Embedded.Tests.RichPreparationBenchmark.h"
#include "FailureHost.h"
int main()
{
    DxUiSample::FailureHost::Host host;
    if (FAILED(host.StartBesideExecutable()))
        return 1;
    const DxUi::Failure::Scope failureScope;
    return RunRichPreparationBenchmark();
}
