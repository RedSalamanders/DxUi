# Original canonical Default reports

All five files are the unchanged library-owned completed-frame WARP Default
workload. Every comparison is unpaired. These reports bind functional runs to
their source, compiler/dependency/environment, executable and build identities;
they establish no performance non-regression verdict.

- Release `80e1517c...`: before the implementation, with the generated oracle
  present. Source F356B6004A05A98330EBCAFE4725FABAFCA4DF9366579B383D1983492007ACAF;
  the subsequent corpus/allocation tests fail as retained in Raw.
- Release `6d2d845a...`: first six-scope success. Source
  14B21EC797E2E192FDDBFCCDEDA0534AD4B9F36D1E2D1741DDB0E8F9376634E0.
- Release `90a99205...`, Debug `2d84aa27...`, ASan Debug `86f04972...`: final
  source 55DE3A71D32382F2DE7DC23D3A7E4EED6A3CDE40F93684C45FF83880F47E9C85,
  linked to the 57 ordinary native suite receipts by the packet verifier.

Full identities and every raw round remain in each report. The original
baseline, historical warnings, unstable controls and acceptance HOLD are
retained in the [parent record](../README.md).
