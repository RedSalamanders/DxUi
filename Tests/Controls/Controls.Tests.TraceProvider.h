#pragma once

// The trace provider (Specs/Core/Core_PerformanceAndResources.md, "Trace provider"): its dormant gate, the events of a
// native Grid scrolling scenario and input-to-present per input, and the same events read back through a real-time ETW
// session where this desktop session may start one.
void TestTraceProviderIsDormantUntilSomethingListens();
void TestTraceProviderPairsEveryGridScrollInputWithItsPresent();
void TestTraceProviderEventsReadBackThroughEtw();
