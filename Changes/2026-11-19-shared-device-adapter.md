- The window-hosting contract now records why `ControlHost`'s per-thread graphics pool creates its device on the
  default adapter instead of the adapter of each window's monitor: every host on the thread shares that device, a
  window can cross adapters at any time, and a message-driven host presents only invalidated frames. This is the
  by-design outcome that RedHome register row H-17 (CV-6-08) allows for DxUi. Documentation only; behavior unchanged.
