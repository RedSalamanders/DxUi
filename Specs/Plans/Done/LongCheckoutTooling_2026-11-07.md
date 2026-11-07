# Long-checkout tooling diagnosis — completed scope

The D-T1 long-checkout diagnosis is complete. MSVC provisioning now refuses an
unqualified long bootstrapper path before side effects, and the evidence fixture
stages classic compiler resources in a shorter owned directory before verifying
and executing the exact copied image at its actual profile path.

Counterexamples, original aggregate failures, successful bounded results and the
unattributed historical hard-link failure are kept distinct in the
[qualification record](../../../Measurements/LongCheckoutTooling_2026-11-07/README.md).
The durable rules are in
[build path admission](../../Build/Build_ConsoleAndPaths.md#native-tooling-path-admission)
and [validation fixtures](../../Testing/Testing_Validation.md#native-tooling-path-fixtures).

This scope does not close the parent remaining-work plan, qualify longer native
tool paths, or establish native/platform/performance acceptance.
