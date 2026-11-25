- Changed popup painting and the native device stack of embedded applications (performance and motion plan W2.4).
  - Popup painting: the `ComboBox` list and native menu popups share one surface painter that keeps, per popup, the shadow
    passes' masks and effects, the backdrop layer's rounded clip, the backdrop blurred once into a bitmap, and the glaze and
    shade gradient stops and brushes, instead of creating them on every paint; the backdrop layer is Direct2D's pooled one.
    A steady popup frame makes no effect and runs no blur pass, and a blurred popup or a menu popup draws its shadow from a
    bitmap both passes were rendered into once; only a `Solid` `ComboBox` list over the view's content still runs its two
    shadow passes. Direct2D's blur result depends on the paint clip, so the blur is kept rather than drawn per paint: a frame
    that repaints a region over a blurred popup now equals the whole frame. Pixel change: a blurred popup (`Mica`,
    `MicaAlt`, `Acrylic`) may differ from earlier builds by one level on some pixels; `Solid` popups are unchanged. A popup's
    blurred backdrop is the content captured when it opens (and when a menu popup moves or resizes) and does not follow
    content that changes behind it, until plan item W5.12. Every retained object is keyed by what it was made from and is
    released on device loss or replacement, DPI change and popup close; registry rows `ComboBox._popupSurfaceCache` and
    `MenuPopup.surfaceCache` count and drop it. `TransientSurfaceBackdrop` gains a retained clip and blurred capture and
    `ReleaseRetainedBlur` (additive), which `PaintTransientSurface` uses the same way.
  - One render per menu open: a native host whose first frame `RenderInitialFrameForShow` presented no longer renders it
    again at the show while it is current (`frame.showPaintsValidated`), and `MenuPopup` no longer invalidates its window
    after showing it; the pacer holds nothing after the open.
  - Native device stack (budget G10): while an `EmbeddedHost` is attached on a thread, the thread's native device stack
    (D3D11, DXGI, Direct2D and DirectComposition devices) is kept after its last native host or popup detaches, so each
    later native menu of an embedded-only application opens without creating a device (about 100 ms per menu on the
    hardware measured, for about 15 to 19 MiB of private bytes and 8 MiB of video memory held). It is released when the
    last such view detaches with no native host attached, at thread exit, on device loss, by the retained-resource drop
    (registry row `SharedDeviceStack.retained`) and by the new `DxUi::ReleaseRetainedNativeDeviceStack()` (additive).
    Nothing is created early, and the embedded host still creates and uses no device of its own.
