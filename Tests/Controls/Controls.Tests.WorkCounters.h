#pragma once

// Work counters (Specs/Core/Core_PerformanceAndResources.md, "Work counters"): the record, its deltas and churn, the
// counted creation helpers, destruction tracking, and the frame, walk and UI Automation counts of a native host.
void TestWorkCounterScopeReportsExactDeltasAndChurn();
void TestCountedCreationHelpersCountEveryResourceKind();
void TestResourceLifetimeTrackerCountsDestructionAtSweep();
void TestWorkCountersCountNativeFramesWalksAndWakes();
void TestWorkCountersCountAccessibilitySnapshotBuilds();
