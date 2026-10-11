# Iteration and negative proofs

These records preserve development failures and corrections. The production wrapper initially accepted six
malformed confirmation/policy results; the strengthened tests reject ten malformed variants. Intermediate
tooling runs fail before the final 32/32 pass, including a PowerShell scalar-argument splat bug corrected to an
array. The early private Debug check and these red/green logs are focused iteration evidence, not final native
qualification. Final candidate execution and complete tooling are retained in the adjacent profile leaves.
The affected selection uses `Test-Changes.ps1 -Explain`; the format check covers the unchanged final native code.
Copy bindings preserve each exact generated original. No failure is relabelled as a pass.