- Native text input cancels previews when editors become read-only or masked and when their host hides or suspends
  at zero size. Late IME messages cannot restart an inactive HWND or steal focus. Cancellation preserves newer
  application text, blocked replacement messages reject their pointer before reading it, and sequential completed
  TSF compositions retain separate undo units.
