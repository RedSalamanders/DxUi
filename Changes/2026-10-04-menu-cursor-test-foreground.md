- `TestMenuChoosesTheCursorWhenItOpensAndCloses` no longer fails when another application takes the foreground while
  it runs. Its menu activates its popup, and a takeover dismisses the menu as designed: under AddressSanitizer the
  desktop application hosting a developer's session took the foreground back before the test had read the popup
  ("read the menu popup rectangle", twice on 4 October). The test now plays its attempts through
  `RunUntilForegroundHeld` with windows of its own, records its first failed expectation and lets the attempt whose
  owner kept the foreground decide, so a regression still fails; a takeover in every attempt records a capability skip
  that names the application. Every attempt puts the physical cursor back, where a failure used to end the process
  with the pointer moved.
  - **Evidence.** A takeover simulated after the menu appears (`SimulateForegroundTheftForTest` on the popup and the
    owner) fails the test as it was, with the same message. With the change, a takeover in the first attempt repeats it
    and passes, one in every attempt records the skip, and a menu that closes itself with nobody taking the foreground
    still fails at once.
  - Specified in `Testing_Validation.md`.
