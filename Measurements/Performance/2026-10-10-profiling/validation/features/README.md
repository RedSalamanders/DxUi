# Four native profiling feature cases

The predeclared inventory contains Debug Default without heap sampling, Debug MultilineGrid with heap sampling,
ASan Default without heap sampling and ASan MultilineGrid with heap sampling. All four have 280 frames, five
windows, twelve stages and complete telemetry. Original native bytes, stamped receipts, child/log/capture hashes
and scenario captures are preserved; the lead independently verifies the fields and decodes both distinct images.

Debug records dirty `01f1aed1`; ASan records clean `cc77bec2`. Full helper/build/profile/artifact identities remain
authoritative. See the [validation inventory](../README.md). These feature checks are separate from the fixed
Release pilot and supply no paired regression, policy, consumer or foreground qualification.
