- DxUi's private window messages are registered by name instead of being `WM_APP` offsets, so an application's own
  messages on the window it shares with DxUi can no longer collide with them: RedSalamander's test message at
  `WM_APP + 0x6A` was DxUi's accessibility action. All sixteen (`src/Support/WindowMessages.h`) now come from
  `RegisterWindowMessageW` as `RedSalamanders.DxUi.<Component>.<Purpose>.v1`, with values in 0xC000–0xFFFF that no
  application message can take: the five that reach the application's window (the accessibility action and provider
  creation, the process-exit detach, the end of a focus gain's turn and a modal menu's menu-bar hover), the ten that
  menu popups receive, and `TextInputServices`' deferred lock, which was registered already and takes the new name.
  Each is registered once, on first use, and every sender and receiver compares that cached value. A failed
  registration is 0 (`WM_NULL`): no sender posts or sends it, each one behaving as when its post fails, and no
  receiver matches it, since a registered message does not compare with a message value and only `Matches` recognizes
  one. `TextInputServices::Attach` reports such a failure as `E_FAIL` rather than a stale `GetLastError`, which could
  read as `S_OK`. A window procedure forwards every message, registered ones included, to `HandleMessage`; DxUi
  reserves no `WM_APP` value, so consumers can drop their reservations of `WM_APP + 0x6A` to `0x6D` and `0x539`. An
  application that drove a modal menu's menu-bar hover by posting `WM_APP + 0x539` itself, as RedSalamander's main
  menu does, calls the new `ContextMenu::PostMenuBarHover` instead. The WindowHost suite checks that every message is
  registered, distinct and never `WM_NULL`, and sends and posts application messages at each former value to show that
  they reach the window procedure while `HandleMessage` neither consumes nor acts on them; the Menu suite shows the
  same for `WM_APP + 0x539` during a modal menu. API revision stays 2 (additive `ContextMenu::PostMenuBarHover`).
  Nothing visual changed, so the gallery is unchanged.
