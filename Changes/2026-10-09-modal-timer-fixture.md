- 2026-10-09: Start the unrelated owner-timer regression's safety deadline after popup discovery and report timer
  and dismissal state on failure, while retaining the required timer-id-1 dispatch assertion. Re-enter TSF activation
  after deliberately retiring the fixture's document instead of only synchronizing its existing native cache.
  Make submenu setup nonactivating and diagnose activation failures without unrelated window names. Let a posted
  mnemonic complete before success cleanup, retaining bounded failure cleanup and the exact command assertion.
