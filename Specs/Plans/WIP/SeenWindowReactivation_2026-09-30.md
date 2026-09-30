# Announcing the reactivation of a window UI Automation has seen, and focus tests that survive foreground theft

Status: ACTIVE (2026-09-30). Library and tests; no consumer pin changes.
Base: the reliability follow-ups (PR 31, `1d6650e`).
Owning contracts: [input and accessibility](../../UI/UI_InputAndAccessibility.md) and
[testing and validation](../../Testing/Testing_Validation.md).

The [reliability follow-ups](../Done/ReliabilityAndFollowUps_2026-09-30.md) (item 7) traced how UI Automation answers a
window host's focus events with an in-process client on Windows 11 build 26200. It answers the first focus event of a
window with a call of the fragment root's `GetFocus`, and later ones from whether the window's root element has the
keyboard focus, which it does not while the focus is inside a control. The host never announces what a gain focuses or
restores, leaving it to the system's event. So switching back to a window UI Automation has seen (Alt+Tab) was reported by
nothing, and a screen reader did not say which control had the focus. That plan recorded this as a gap it left open.

## Execution

- [x] 1. The host announces what the gain of a seen window focused or restored. Accept: when a window whose fragment root
  had been asked for its focus before gains focus again, the host announces the focused element once the gain's turn
  ends: the restored control, the control the activation focused, or the window when no control has focus. It announces
  nothing more when it already announced a move of that turn (the click of item 7), and nothing when the root element
  stands for its single focused control, which UI Automation reports itself. A window's first gain stays the answer of
  its first `GetFocus` call, with no duplicate. A desktop-free WindowHost test decides each case, and a Menu-suite test
  with an in-process UI Automation client reactivates a seen window and hears its control once. Each test fails
  without the change it covers.
  Outcome: the host reads the count of fragment-root `GetFocus` calls as a gain begins, before it publishes anything. It
  reads it there rather than after the publish because UI Automation asks only for a window's first event: a call that
  begins during the gain is this gain's own answer, and counting it would add a duplicate to a first activation. When
  the count is not zero, the host announces the focused element as the turn's message is dispatched, unless it already
  announced a move of the turn, or `RootElementHasKeyboardFocus` (the root provider's own answer) is true. The WindowHost
  suite decides the restored and the newly focused control, a first gain, a click of the turn and a single-control root
  without a desktop. The Menu suite's real client heard the restored and the focused control once each, three runs in a
  row, and item 7's click tests pass unchanged. Four mutants each fail the WindowHost test they target: no announcement,
  a first gain announced too, a click of the turn announced again, and a focused single-control root announced. The
  first of them also fails the real-client test.
- [x] 2. A turn ends only with its own message. Accept: the message of an earlier gain, which the window lost before the
  loop turned, arrives during a later turn and ends nothing; a desktop-free test dispatches it on its own.
  Outcome: the message carries the turn's number in `wParam`, and a message whose number is not the running turn's, or
  that arrives with no turn running, ends nothing. Letting any message end the turn fails the test.
- [ ] 3. The real-client focus tests never fail because another application took the foreground. The Menu suite's click
  and reactivation tests use two windows and a UI Automation client; one failed a local Debug run while the desktop app
  and the Start menu took the foreground. Accept: each attempt builds its windows afresh and records the first
  expectation that failed. An attempt that another application took the foreground from, from any of its windows, is
  played again, at most five times, and a test whose every attempt lost it records a skip naming the thief. The
  expectations of the attempt that kept the foreground decide the test, so a regression still fails.
  `--foreground-thief` never produces a failure, and the tests still fail against the mutants above and item 7's.

### Close-out

- [ ] `test.ps1` in x64 Debug, Release and ASan Debug, the three ARM64 builds, `validate.ps1` and `format.ps1 -Check`;
  the input and accessibility contract, the hosting guide and CHANGELOG; this plan moved to Done.
