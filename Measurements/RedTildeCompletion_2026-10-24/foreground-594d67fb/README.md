# Final SDK foreground qualification — 25 October

The [reviewed foreground packet](summary.json) records one x64 Release run at source commit `594d67fb`.
Menu, NativeTextInput, MenuResources and MenuResourceScaling each pass with zero capability skips. The person
confirmed the normal Start prompt; automatic start was disabled. The shared lease completes with foreground
and keyboard focus unchanged and the pointer restored.

Independent replay verifies all 17 completed payload files and their byte lengths/SHA-256 digests. Actual
ControlTests, EmbeddedTests and InteractiveLease executable bytes match the retained `594d67fb` Release matrix;
the build-attestation digest and identity also match. The structured packet retains the completion,
independent-audit, executable and build-identity digests without publishing private paths or desktop details.

The accompanying performance preflight is unpaired. Its source commit and production fingerprint match the
qualified SDK revision, but it records a dirty checkout. At audit time the changes were documentation and
evidence only; that report does not retain a complete dirty-path inventory. This limitation is preserved.
Its six benchmark input hashes identify the library-owned Complex UI fixture; they are distinct from executable
hashes and the complete four-suite source closure.

This run is separate from the local noninteractive matrix, hosted native execution, paired performance,
RedTilde keyboard qualification and physical IME/touch/assistive-technology acceptance. No quiet-host,
performance-policy or whole-product acceptance is claimed.
