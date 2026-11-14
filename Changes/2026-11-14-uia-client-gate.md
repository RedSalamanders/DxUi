- UI Automation client gate (performance and motion plan W8.1): one rule for window hosts and embedded views. A host
  builds its accessibility snapshot on a mutation or update only when a client has requested its provider during this
  attachment, or `UiaClientsAreListening` reports a listener; once requested it stays so for the attachment. Window hosts
  keep their behaviour from before the item. An embedded view that nobody has asked while nobody listens now only records
  each changed `UpdateAccessibility` (calling it every frame builds nothing); its first provider call builds the recorded
  update when the geometry is still prepared, then every changed update builds and raises as before; events are raised
  only from `UpdateAccessibility` (a delta a first request owes a listener is raised by the next update, even an
  unchanged one), and a recorded update no longer walks the tree for its preparation count. A first request that cannot
  build (resized or unprepared view) serves the snapshot built at attachment until the next update. Embedded provider
  requests and GetItem materialization no longer cancel a pointer gesture. UI Automation's advise calls are not used:
  it advises no desktop-scoped subscription and no focus handler (measured, see the contract). Covered by the
  client-gate cases of the Accessibility suite; the cost remains for any host a client has ever asked, and for every host
  while another process on the desktop listens. Narrator and Inspect acceptance remains open.
