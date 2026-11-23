#pragma once
// One way to assign a property (plan W2.10) on FX5: the rows of Support.Tests.AssignSetterRows.h on the ComplexUi scene of
// Samples/ComplexUi/ComplexUiScene.h and its embedded host (the window host run is Controls.Tests.AssignSetters.h).
// Included by Embedded.Tests.Embedded.cpp, which defines Check and Hr.
#include "Support.Tests.AssignSetterRows.h"

namespace AssignSetterGate
{
__declspec(noinline) inline void TestFx5EverySetterAssignsOnce(GraphicsFixture& gpu)
{
    g_require = &Check;
    ComplexUiScene scene;
    Hr(scene.Initialize(gpu.device.get()), "FX5 scene for the assignment gate");
    Hr(scene.view.Prepare(1280u, 720u, 96.0f), "FX5 first frame");
    Scene rows;
    rows.root = dynamic_cast<DxUi::Panel*>(scene.view.Controls().GetRoot());
    Check(rows.root != nullptr, "FX5 root is a panel");
    rows.model    = &scene.model;
    rows.sliders  = {scene.sliders[0], scene.sliders[1], scene.sliders[2]};
    rows.progress = scene.progress[0];
    rows.group    = rows.root->AddChild<DxUi::RadioButtons>();
    static_cast<void>(RunRows(rows, scene.view.Controls().GetRoot(), true, "FX5 embedded host"));
    // The scene still renders after every row: its next frame prepares without failure.
    Hr(scene.view.Prepare(1280u, 720u, 96.0f), "FX5 frame after the assignment gate");
}

__declspec(noinline) inline void TestAssignEqualityOnEmbeddedSuite()
{
    g_require = &Check;
    TestAssignEquality();
}
} // namespace AssignSetterGate
