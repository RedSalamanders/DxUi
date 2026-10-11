- Embedded structure notifications use a bounded coalesced delivery slot and a COM-marshalled provider
  proxy on an MTA. Public embedded providers retain their STA/site contract. A held UIA client callback no longer
  runs inline on the publishing owner or retains historical prepared row snapshots. Disconnect revokes registration
  and clears pending delivery without joining clients. API revision 3 and the public C++ interface are unchanged.

  Library completion is recorded in `Specs/Plans/Done/PreparedTreeAccessibility_2026-10-06.md`; continuing performance,
  foreground and consumer qualification stays in `Specs/Plans/WIP/ProductionReview_2026-10-05.md` and its consumer
  handoff register. Actual consumer callbacks and current paired acceptance remain required before adoption closes.

  - Recorded the x64 Debug/Release/real ASan test matrix and three ARM64 cross-builds for the pushed structure-delivery source, including independent readback of 1,394 archived files. Current-source paired scenes and RedPrism client qualification remain open.
