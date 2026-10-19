- Add an explicit native/embedded semantic Markdown rendering preview with
  cooperative paragraph/table geometry, mapped source range painting and scrolling,
  atomic token deletion and observable source fallback for admission overflow.
  Complete structural editing, source caret sites and semantic UIA remain in progress.
  Keep native geometry and retained v0/semantic line/rectangle queries recoverable
  under allocation failure, including MSVC Debug container bookkeeping and omitted
  return-value optimization; retain the failing receipts and add complete sweeps.
