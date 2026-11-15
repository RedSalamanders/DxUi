#pragma once

// Text history by bytes (plan item W9.3; Specs/Core/Core_PerformanceAndResources.md, "Single-line text history"). A
// deterministic edit script on a TextField and an editable ComboBox records the text, caret and selection after every
// operation, every undo and every redo, and compares them with the sequence the full-text snapshot history recorded at the
// plan's base commit (Controls.Tests.TextHistoryExpected.h): undo granularity and restored state are unchanged.
void TestTextFieldHistoryMatchesSnapshotHistory();
void TestComboBoxHistoryMatchesSnapshotHistory();
