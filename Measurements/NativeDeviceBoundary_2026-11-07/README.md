# Native device-creation exception boundary

The [original triage packet](triage.json) binds the hosted Debug failure to exact
EXE/PDB, dump and record hashes. Original wrappers and every canonical payload member
were independently byte-verified. Same-image symbols identify the direct
`D3D11CreateDevice` attempt in `CreateD3D11DeviceWithWarpFallback`.

The PAC-aware native ARM64 walker had already verified both named Grid callback origins
with four PAC attribute queries each. A later ordinary paint test failed during native
device creation. The original driver's HRESULT is unknown because the observer terminated
before the native implementation returned. This does not assign a cause to the older
output-less Grid failure.

A controlled native factory raises and handles its own exception, then returns a checked
HRESULT. The retained patch-object baseline terminates with 0xC0000409; the candidate
passes hardware/WARP and optional debug-layer retry cases without changing HRESULT/output
semantics. Probe source, EXE/PDB, first failures and raw logs are privately retained with
independent archive-member verification. Public API and ABI remain unchanged.

The normative boundary belongs to [failure handling](../../Specs/Core/Core_FailureHandling.md)
and [native hosting](../../Specs/Rendering/Rendering_Win32Host.md). Complete current-source
native matrix and hosted ARM64 acceptance are separate from this diagnosis and focused proof.
