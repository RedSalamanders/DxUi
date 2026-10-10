- Publish native accessibility state lazily and coalesce mutation batches while keeping owner queries fresh and focus
  transitions synchronous. Bind snapshot-derived peers to the inspected control identity, release action locks before
  callbacks, and reject delayed visual-line operations when concurrent endpoint or document edits invalidate their input.
  Preserve current text in retained native ranges and prevent stale attachment messages from publishing into a new host.
