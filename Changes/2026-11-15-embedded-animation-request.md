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
  sends no `WM_SETTINGCHANGE`. An advance before the earliest deadline runs no wake. `capabilities.json` lists consumer
  adoption of revision 6 as pending. `EmbeddedCallbacks::nowTickMs`, the application's clock, is required at `Attach`
  (`DXUI_HAS_EMBEDDED_APPLICATION_CLOCK`): a stamp taken between advances (a keystroke, a resting pointer, a transition
  started from input) reads it, so an application that sleeps until its request no longer sees a tooltip shown early, a
  caret flipping right after a keystroke or a transition jumping on its first frame. The view records the request it
  last reported, so a cancellation that leaves a later deadline cannot hide a sooner need from a sleeping application,
  and a deadline beyond the end of the application's clock is reported as `None` instead of making a loop spin.
