- 2026-10-09: Retire native IMM previews immediately after application text replacement and reject late payloads
  until a fresh composition starts. Keep text and composition chosen by reentrant result callbacks, including
  focus transfer, and preserve the committed result when its callback changes editors. Honor no-GCS composition
  cancellation while preserving earlier undo history and rejecting late payloads.
  Cancellation rechecks editor lifetime and session ownership after virtual state reads and contains standard
  exceptions without erasing a successor.
  The eligibility-transition regression explicitly acquires HWND keyboard focus and starts its IMM transaction
  before checking preview cancellation, matching the native host's input prerequisites.
