- 2026-10-09: Dispatch each posted Unicode mnemonic before inspecting the same-thread popup state, verify duplicate
  matches do not invoke commands, and assert the selected closing command. Correct the IMM application-replacement
  fixture to verify insertion at its preserved caret rather than assume SetText moves the caret to the end. Verify
  RTL submenu visible-surface anchoring and left preference independently of overlapping native shadow bounds.
