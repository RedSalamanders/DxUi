- Make the TSF text store follow the documented query semantics. `GetACPFromPoint` honors `GXFPF_ROUND_NEAREST`
  and `GXFPF_NEAREST` and reports `TS_E_INVALIDPOINT` for a point outside every character box instead of an invalid
  position. `GUID_PROP_INPUTSCOPE` is served through the attribute requests, and the new additive
  `TextField::SetInputScope` (`Default`, `Number`, `Url`) reaches TSF; NumericStepper fields report `Number`. External
  edits notify `TS_TEXTCHANGE` without the common prefix and suffix, never splitting a surrogate or CRLF pair. The
  native store reads `GUID_PROP_ATTRIBUTE` after each edit session and publishes the conversion target and clause
  boundaries through a pure, tested mapping instead of clearing them on every range update.
  - Validation: new non-activating TextField-suite cases in `Controls.Tests.TextStoreComposition.h`. The real Japanese
    IME check under `test.ps1 -Interactive` and the focus-taking NativeTextInput suite remain for the developer.
