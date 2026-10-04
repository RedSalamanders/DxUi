- Slider takes the thumb of the 9 September touch review and adds touch feedback. Its accent thumb is visible at rest:
  14 DIP (20 hovered, 16 pressed) in a 24 DIP gray disc, where it was 6, 16 and 12 DIP in a 20 DIP disc. While a touch
  contact drags the slider, a 48 DIP touch halo, the accent at 24% (an opaque 2 DIP ring in high contrast), grows from
  the disc to the grab area around the finger that covers the thumb and shrinks away after the release. A cancelled drag
  removes it at once, and the mouse and pen never show it. The hit band and the grab radius are unchanged.
  - **Pointer device.** `ControlHost::GetPointerDevice` reports the device of the pointer event being handled
    (`PointerDevice::Mouse`, `Touch` or `Pen`). A window host reads it from each mouse message's extra information, which
    Windows marks for the touch and pen contacts it promotes (`PointerDeviceFromMessageExtraInfo`); an embedded host
    takes the new `PointerEvent::device`, the mouse when an application names none, so existing initializers are
    unchanged. API revision stays 3: these are additions.
  - **Tests.** Animation: the halo's growth, settling at 48 DIP, release, cancellation and seek, a mouse drag without it,
    and reduced motion snapping it. WindowHost: the classification of the extra information, and a touch-marked message
    that drags a slider. Embedded: `PointerEvent::device` reaching the host and the slider. The geometry checks of
    Animation and NewControls follow the new thumb.
  - **Gallery and design system.** A new `Slider / Touch pressed` tile, and every sheet regenerated. The Slider
    guideline, preview and tokens follow; the published design system is republished after the merge.
  - Specified in `UI_ControlsAndLayout.md` and `UI_InputAndAccessibility.md`, with `docs/controls.md` and
    `docs/hosting.md`.
