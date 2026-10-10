- Honor disabled native focus transfer for editors, including idempotent logical focus changes; keep OS text services
  bound to the actual focused HWND. Track embedded preview ownership so hide, zero-size and focus cancellation restore
  the entry text while preserving newer application text. Add focus regressions and reconcile nonactivating fixtures.
