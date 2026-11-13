- Grapheme next-step and caret snapping no longer segment from the start of the text (`W8.6`).
  `StepToNextTextElement` off the ASCII path and `SnapCaretIndexToTextElementBoundary` now search backward from the
  caret to the nearest pair that UAX #29 always breaks (GB4/GB5, or GB999 with GB3, GB6-GB13 excluded by the pair),
  through the new shared `FindTextElementSearchAnchor`, then walk forward; `FindPreviousTextElementBoundary` uses the
  same, wider anchor rule, which also stops between Hangul syllables and Indic consonants. Boundaries are unchanged:
  the 766-vector Unicode 17 corpus passes unchanged and a new differential test compares next, previous and snapping
  with the from-zero segmentation on that corpus and 4,096 seeded mixes, including lone surrogates. On 65,536 units of
  CJK and emoji a right arrow in `TextField` went from about 53,000 code units visited to at most 24, and a UI
  Automation character expansion reads at most 63 (gate 64); receipt in
  `Measurements/PerformanceAndMotion_2026-11-13/W8.6`. Regional-indicator runs, pictographic ZWJ chains and Indic
  linker chains still read back to their start.
