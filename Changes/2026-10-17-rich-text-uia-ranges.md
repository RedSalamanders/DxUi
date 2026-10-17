- Implement RichTextEditor source UIA range scrolling, contiguous selection
  add/remove, ordinal Unicode text search and uniform read-only attribute search.
  Use prepared wrapped lines for range movement; validate arguments and preserve
  source/history, host ownership and retirement guards. Add native/embedded and
  real-client tests and document single-selection rejection rules and remaining
  rich semantics/platform/performance gates. Gallery visuals are unchanged:
  commands reuse existing scrolling and selection rendering.
  Other text controls without these optional range actions report unsupported
  instead of a success-only no-op.
