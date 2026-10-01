- API revision 3, with a rule for when the revision changes and a validated list of what consumers may call.
  - **Why.** Three changes since revision 2 could break a pinned consumer, yet the revision stayed at 2. A consumer moving its
    pin to this commit or a later one sets `apiRevision` to 3 in `Dependencies/DxUi.lock.json` and makes these changes:
    - **Renamed interfaces (#29).** `IDxGridModel`, `IDxGridDelegate`, `IDxTreeModel` and `IDxTreeDelegate` are now
      `IGridModel`, `IGridDelegate`, `ITreeModel` and `ITreeDelegate`.
    - **No Python tools (#30).** A consumer that ran the Python build-matrix validator runs
      `validate-build-matrix.ps1 -Root <consumer root>` instead.
    - **Registered window messages (#41).** DxUi's private window messages are registered by name, so a consumer:
      - drops any DxUi `WM_APP` value it copied;
      - forwards every message to `HandleMessage`, including registered ones (0xC000–0xFFFF);
      - posts the menu-bar hover with `ContextMenu::PostMenuBarHover`.
    - **New output fingerprint.** The consumer output fingerprint includes the revision, so the first build after the
      move uses a fresh dependency output directory.
  - **The rule** (`Specs/Build/Build_ToolchainAndConsumption.md`).
    - The revision increments when a consumer built against the previous one could stop compiling, linking or behaving as
      before without changing its own code. Additions do not count.
    - Instead of incrementing, a change may keep the old form working for one revision. The alias has no `[[deprecated]]`
      attribute, because consumers compile with warnings as errors; the changelog names it instead.
  - **The consumer interface.** `capabilities.json` now lists what a consumer may call or import:
    - three scripts, with the parameters consumers pass;
    - five module functions, with their parameters;
    - the four MSBuild entry files;
    - the public header root.
    `validate-dependencies.ps1` fails when a listed entry disappears, a listed function is no longer exported, or a listed
    parameter is renamed. It reads scripts and modules from their parse trees, and new parameters pass.
  - **Single source for the revision.** `Get-DxUiConsumerBuildIdentity` and `test-consumer.ps1` read the revision from
    `capabilities.json` instead of hard-coding 2. The docs, the consumer-integration skill and the README say revision 3.
