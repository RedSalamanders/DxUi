- `Control::Measure` now caches each control's results, shared by every parent (performance and motion plan item W6.2):
  two slots keyed by exact constraints within one context (measure revision, host, typography revision, DPI, flow and
  density), out of line so a never-measured control pays one pointer. `FlexPanel`'s per-child measurement records and
  `FormField::_measureCache` are gone. A new protected `Control::MeasureDependsOnMaximum` (default `true`) lets a
  non-wrapping control reuse one size across maxima; `Label`, `Button`, `Checkbox` and `TextField` while they do not wrap,
  `Toggle`, `RadioButton` and `ComboBox` declare it. The public contract now states that a control must call
  `InvalidateMeasure` whenever anything its `MeasureOverride` reads changes; a Debug-only verification mode re-runs the
  measurement on every hit in the layout tests. Measure invalidation was added where a measured input changed silently
  (a button's first disclosure state with a content font size; a masked multiline field's reveal state, mask policy and
  focus-time mask epoch). New `layout` work counters count hits, misses, evictions, executions and verification
  mismatches. On `FX10` no MeasureOverride runs for an unchanged key; `FX12` measures nothing. Every control is 8 bytes
  larger (the size probes were raised with the reason).
