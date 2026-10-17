# Setup and refusal evidence

Exact original setup/build/format/schema/validation logs, overlay records/backups,
pre-change Default baseline and scheduler preflight attempts. JSON payloads use
`.receipt.txt` aliases; restore the original suffix only in a disposable replay
directory if a driver requires it. This includes the unpaired baseline comparison,
which is no acceptance verdict. The first formatted build copies and the initial
format-check failure remain visible alongside their corrected successful builds.
The full validate entrypoint forwards child output to its host; its retained log
contains the six final passing step statuses, while the profiling-specific log
retains all 125 schema/runner case results. The tool transcript retains the full
child console stream. Subsequent complete validation is captured by an outer
process in the indexed-document qualification packet.

