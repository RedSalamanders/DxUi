# Native device-creation exception boundary — completed local scope

Status: complete for the D3D11 native-call boundary and local qualification.

Native D3D11 implementations may handle C++ exceptions internally and return an HRESULT.
Each hardware/WARP creation attempt, including optional debug-layer retry, uses the audited
native ABI allowance. Exceptions escaping native code and reentrant SDK/application callbacks
retain fatal observation. Public API/ABI and checked result/output semantics are unchanged.

The lasting contracts are [failure handling](../../Core/Core_FailureHandling.md) and
[window hosting](../../Rendering/Rendering_Win32Host.md). Original hosted ARM64 EXE/PDB/dump
diagnosis, controlled before/after failures, six zero-warning builds, 66 ordinary native x64
suite executions, actual ASan and independent original-member verification are in the
[source-bound packet](../../../Measurements/NativeDeviceBoundary_2026-11-07/README.md)
at `c4272b0d` (API 5).

Native ARM64 execution and the complete hosted production gate remain in the
[single current plan](../WIP/RemainingWork_2026-11-04.md). The earlier output-less failure
remains unclassified; the later PAC verifier successes do not qualify the entire ARM64 suite.
