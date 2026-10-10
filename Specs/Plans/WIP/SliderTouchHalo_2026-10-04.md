# Slider touch thumb and halo

- **Status**: ACTIVE (9 October 2026). The library implementation merged in #62 and the hosted paired comparison passed
  before merge (run 37201375166). Physical-touch qualification and the accepted Q26 clipping/overlap checks remain;
  consumer API-revision-4 pin and `PointerEvent::device` adoption are tracked separately in
  [the handoff register](ProductionReview_2026-10-05/api4-consumer-handoffs.md).
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
- [x] The hosted paired benchmark's verdict on the pull request: passed before merge in run 37201375166. On 9 September this geometry measured a median clean
  private-byte rise of about 0.5 MiB on the branch, which the developer accepted then; the halo paints only during a
  touch drag, which no benchmark scenario makes.
- [x] Republish the design system artifact (version 19): the new gallery sheets, and the Slider guideline, preview,
  preview styles and tokens, with `design-system.json` naming the new uploads in the repository too.
- [x] Merge in #62.
- [ ] Add/verify parent and viewport clipping for the halo and record visual-overlap plus hit-test behavior near
  neighboring controls and menus, as accepted by Q26. The current spec permits painting beyond the control bounds;
  this review has not qualified overlap behavior.
- [ ] Qualify the behavior on physical touch hardware; synthetic `PointerDevice::Touch` and message-extra-info tests are
  implementation evidence, not device acceptance.
- [ ] Consumers pass `PointerEvent::device` from their embedded input in their own API-revision-4 pin/adoption changes;
  see the handoff register.

## Validation

On `b1099d2` (the gallery commit `39a5da4` and this record change no source):

- `validate.ps1` and `format.ps1 -Check` pass.
- `test.ps1` with the 19 suites that do not take the desktop passes in x64 Debug and Release. In x64 ASan Debug 18
  pass, the slider's Animation, WindowHost and Embedded tests included, and NewControls stopped once in
  `TestMenuChoosesTheCursorWhenItOpensAndCloses` (its menu popup had already closed), as it did on the merged tree of
  #60, which has no slider change. The whole suite passed under ASan when run again, while its log recorded the desktop
  in use: a window covering the menu cursor tests, and the pointer moved during one of them.
- ARM64 Debug, Release and ASan Debug build. They are cross-builds: native ARM64 execution stays with CI.
- `gallery.ps1 -PublishDocs` regenerated the sheets from an x64 Release build of `b1099d2` with a clean tree, and the
  slider tiles were checked in the light, dark and high-contrast sheets.
