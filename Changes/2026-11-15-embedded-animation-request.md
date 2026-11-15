- API revision 6: an embedded view publishes when it next needs the application's clock (`W3.5`).
  `EmbeddedHost::NeedsAnimation()` is removed and `AdvanceAnimation(nowTickMs)` returns an `EmbeddedAnimationRequest`
  instead of a `bool`; the new `GetAnimationRequest()` reads the same request without ticking. A request is `None`,
  `Continuous` (advance every frame) or a `Deadline` whose `dueTickMs` is on the clock the application passes to
  `AdvanceAnimation` (0 when the next advance only takes the baseline after a pause). The clock rules are unchanged, and
  DxUi still owns no timer: an application that sleeps until the request wakes twice per caret blink period, twice per
  tooltip, and never while idle, hidden or after the caret timeout, where one that polls each frame advanced every frame
  while anything waited. The view calls `requestPreparation` when it needs the application sooner than the request it
  last reported. Revision-5 code fails to compile rather than run with a changed meaning; `docs/upgrade.md` maps each
  call, `DXUI_HAS_EMBEDDED_ANIMATION_REQUEST` marks the new headers, and `Tools/ConsumerApi/ConsumerApi.Tests.Revision6.cpp`
  freezes the surface. `ControlHost::RefreshUserSettings()` re-reads every user setting a host keeps (caret blink time,
  caret timeout, mouse hover time) and replaces the unreleased `RefreshCaretBlinkSettings()` and
  `RefreshTooltipHoverTime()`; an application calls it for embedded views and for hosts on child windows, which Windows
  sends no `WM_SETTINGCHANGE`. An advance before the earliest deadline runs no wake. `capabilities.json` lists consumer adoption of
  revision 6 as pending.
