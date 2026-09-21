# V11 foreground interaction qualification

Menu and NativeTextInput pass in x64 Debug, Release and ASan Debug: six complete
runs, zero skips. Each verifies original child focus, foreground and cursor
restoration. `receipts.txt` records exact executable, wrapper and wrapper-source
SHA-256 identities. Logs are unchanged copies; the two source snapshots are
external orchestration evidence, not library dependencies or product source.

The wrapper uses the existing RedSalamander test warning and named desktop lease,
activates only its owned bootstrap and grants foreground only to its created
child. A suspended child is not yet registered with USER (error 87); V4 grants
after startup, with a two-second bounded registration wait. A job and owned-child
timeout guard bound process lifetime. No unrelated process is discovered or killed.

Prior attempts remain under `C:/RedSalamander.Perf/evidence/i26-ui`: V1 has seven
desktop skips; V2 fails the undifferentiated handoff; V3 identifies error 87 before
child execution. None is relabelled as a pass. V4 changes the startup ordering.
These runs do not establish native ARM64, consumer UIA/screen-reader, physical
mixed-DPI or performance acceptance.

The archived source snapshots normalize trailing empty lines. Receipt source hashes
identify the unchanged external originals, retained at the raw evidence location.
