- 2026-10-09: Immediately retire the native editor session, TSF document and cached text when a host focus
  notification removes its target, including a throwing callback. A returned live child loses acknowledged focus.
  Cover destruction and ownership transfer before any message pump can repair stale state.
