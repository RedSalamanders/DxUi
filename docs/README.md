# Using DxUI

DxUI is a Windows C++ retained-control library delivered as one static archive, **DxUi.lib**.
The API uses namespace `DxUi`. It supports a supplied-device embedded view and an HWND-based native host.

- [Getting started](getting-started.md): prerequisites, builds, exact-pin consumption and a minimal control tree.
- [Hosting and lifetime](hosting.md): graphics, input, DPI, animation, native windows and recovery.
- [Control guide](controls.md): all 26 controls, configuration, events and model ownership.
- [Generated gallery](gallery/README.md): five themes and the runnable embedded example.
- [Performance and testing](performance.md): complex-UI FPS, memory, comparisons and ARM64 evidence.

The [public example](../Samples/EmbeddedControls/EmbeddedScene.h) and its
[application host](../Samples/EmbeddedControls/Main.cpp) are compilable references.
The [capability manifest](../capabilities.json) distinguishes supported library behavior from consumer bridges
still to be implemented. Read the [normative specs](../Specs/README.md) when changing library behavior.

Licensed under [MIT](../LICENSE.txt). [Third-party notices](../THIRD-PARTY-NOTICES.md) cover the pinned WIL dependency
and retained source attribution. No font or icon-font file is distributed with DxUI.
