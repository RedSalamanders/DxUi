# Specification authority

`AGENTS.md` and this file are repository policy. Domain documents state intended behavior and required validation;
`capabilities.json` distinguishes implemented support from pending work. Machine manifests own exact versions and
identities. WIP plans sequence unfinished work; Done plans are historical. Tests and implementation are evidence,
not permission to ignore a conflicting contract. Reconcile disagreements in the owning domain.

| Contract | Domain |
| --- | --- |
| [Architecture and ownership](Core/Core_Architecture.md) | Core |
| [Performance and resources](Core/Core_PerformanceAndResources.md) | Core |
| [Toolchain and consumer integration](Build/Build_ToolchainAndConsumption.md) | Build |
| [Embedded D3D11 hosting](Rendering/Rendering_EmbeddedD3D11.md) | Rendering |
| [Input and accessibility](UI/UI_InputAndAccessibility.md) | UI |
| [Controls and layout](UI/UI_ControlsAndLayout.md) | UI |
| [Theme and typography](UI/UI_ThemeAndTypography.md) | UI |
| [Window hosting](Rendering/Rendering_Win32Host.md) | Rendering |
| [Validation and evidence](Testing/Testing_Validation.md) | Testing |
| [Documentation and gallery](Core/Core_Documentation.md) | Core |

Every direct WIP plan is indexed once. Before completion, persist enduring discoveries in the owning domain,
complete implementation and validation, then move the plan to Done and remove its active row. A separate HOLD
consumer migration does not prevent completing an independently scoped bootstrap plan.

Run the three root validation scripts for documentation/dependency changes and the appropriate tests for code.
Generated measurement reports live under `.build`, never in Specs. Reviewed gallery snapshots live in `docs/gallery`.
Historical import records are explained under [Done](Done/README.md). See [the active index](Plans/WIP/README.md).
