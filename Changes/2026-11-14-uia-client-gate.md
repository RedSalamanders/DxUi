- UI Automation client gate (performance and motion plan W8.1): a provider query no longer makes accessibility
  snapshots build on every later mutation. A window host builds at a mutation, and an embedded
  `UpdateAccessibility` builds, only while a client may hear the events: a subscription UI Automation advised on the
  root provider (now an `IRawElementProviderAdviseEvents`) or, since UI Automation advises a window of neither
  focus-changed handlers nor subscriptions made at the desktop (a screen reader's), `UiaClientsAreListening`.
  Otherwise a window host only marks its snapshot dirty and an embedded update only records its preparation and
  placement. Queries stay current: a window-thread query publishes first, a query on another thread asks the window
  thread to publish (bounded wait) before it reads, and an embedded owner-thread query builds when the view is still
  as prepared (else the next update builds). With one client the events are unchanged (FX6 reference). Covered by
  the new FX6 client-gate cases of the Accessibility suite; the assumption that every subscription is advised was
  measured false and is guarded by a probe. Narrator and Inspect acceptance, and the remaining cost when another
  process on the desktop listens, remain open.
  Review follow-up: a publication no client could hear (a query's) now moves the event baseline, so a client that
  reads a state, subscribes and sees it changed back hears the change; a busy window thread costs the queries of one
  unheard revision one bounded wait in all (never while a snapshot mutex is held); an embedded query reads a value an
  input handler set before the next Prepare, keeps snapshot and placement consistent, never cancels a pointer gesture,
  and keeps updates building for a client that keeps querying; one gate decides every event computation; a window an
  MSAA client asked for its client object counts as a listener.
