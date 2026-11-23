- Added the `DxUi` TraceLogging provider `{89b12dac-c17b-5934-baa0-35b9f94990e6}` (performance and motion plan W0.8):
  input received, frame requested with its reason, prepare begin and end, present (composite in an `EmbeddedHost`) and
  wake by source, each with a host number, per-host input and frame numbers that pair an input with the present that
  shows it, and one `QueryPerformanceCounter` clock. Dormant until a session listens (one branch per event site, no
  allocation, lock or formatting); written on the working thread, with no new thread, timer or window in the embedded
  host; identifiers, counts, sizes, reasons and times only. `Tools/Capture-DxUiTrace.ps1 -Start`/`-Stop` captures and
  decodes a trace with the inbox `logman` and `tracerpt`; the schema is in `docs/performance.md`. Tests prove the
  ComplexUi dirty frame keeps its allocations with and without a listener, one input event per input and one present
  event per present on a scrolled `Grid` with input-to-present per notch, and the same events read back through a
  real-time ETW session. Input-to-present on hardware (`G8`) remains to be measured.
