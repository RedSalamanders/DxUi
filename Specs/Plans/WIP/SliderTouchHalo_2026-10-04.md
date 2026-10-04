# Slider touch thumb and halo

- **Status**: ACTIVE (4 October 2026). Implemented and validated locally; the hosted paired benchmark, the merge, the
  design system republish and the consumers' `PointerEvent::device` remain.
- **Owner**: Slider, input hosting, gallery and design system.
- **Scope**: Adopt the thumb geometry of `codex/slider-touch-review` (`2b4f7bf`) on current main, whose behavior fixes
  #25 already ported, and add touch feedback: a translucent halo of the 48 DIP grab area while a touch contact drags
  the slider. The library learns which device a pointer event came from, in the window host and in embedded hosts.

## Why

On 4 October the developer compared main's slider (a 6 DIP accent in a 20 DIP disc) with the branch's (14 in 24) and
chose the branch's look, with a bigger translucent area under touch for better touch feedback: a finger covers the
thumb, so feedback has to show around it.

## Checklist

- [x] Thumb geometry from `2b4f7bf`: a 14, 20 and 16 DIP accent thumb (rest, hover, pressed) in a 24 DIP disc.
- [x] `PointerDevice`, `ControlHost::GetPointerDevice`, `PointerDeviceFromMessageExtraInfo` and `PointerEvent::device`.
- [x] The touch halo: 48 DIP, the accent at 24%, eased with the press, removed on cancellation, an opaque ring in high
  contrast, never for the mouse or a pen.
- [x] Tests (Animation, WindowHost, Embedded) and the updated geometry checks.
- [x] Gallery tile `Slider / Touch pressed` and regenerated sheets; the design system's Slider guideline, preview and
  tokens.
- [x] Specifications and usage documentation.
- [ ] The hosted paired benchmark's verdict on the pull request. On 9 September this geometry measured a median clean
  private-byte rise of about 0.5 MiB on the branch, which the developer accepted then; the halo paints only during a
  touch drag, which no benchmark scenario makes.
- [ ] Merge, then republish the design system artifact with the new gallery sheets.
- [ ] Consumers pass `PointerEvent::device` from their embedded input (RedXe, RedSalamander), each in its own change.

## Validation

The local runs are recorded here when they complete.
