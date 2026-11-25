#pragma once

// Pointer-move cost on the WindowHost (plan W2.9; Specs/UI/UI_InputAndAccessibility.md, "Pointer hit result"): one hit
// test per pointer move on FX14, no preparation membership search and no plain-label preparation on FX10, the seeded
// differential against the uncached hit test, and the hit result's lifetime.
void TestPointerHitFx14WindowHitTestsOncePerMove();
void TestPointerHitFx10WindowPreparesNoPlainLabelAndSearchesNoTree();
void TestPointerHitWindowDifferentialMatchesTheUncachedHitTest();
void TestPointerHitWheelUsesDeliveredModifierState();
void TestPointerHitWindowResultNeverOutlivesItsControl();
void TestPointerHitWindowUnreportedControlIsNeverReused();
void TestPointerHitPlainLabelsAreNotPrepared();
void TestPointerHitWindowCursorQueryUsesTheDeliveredPoint();
void TestPointerHitWindowReportsWhyReuseIsOff();
void TestPointerHitLabelSubclassAndReplainedLabel();
