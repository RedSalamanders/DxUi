# Localized action flow — first candidate

The candidate introduces `ArrangeMeasuredActions` and opt-in multiline standard Buttons. Its
synthetic French fit assertions pass. The first direct Control run recorded 425, 439, 425, 426,
426 us per 24,000 layouts (median 426), versus baseline 847 us. Both use the same measured inputs
and assertions; the adapter changes from StackPanel to the allocation-free flow helper. This is
layout construction cost, not rendering/input performance or proof of the full localized scene.

`performance.json` and `repeat-performance.json` retain common complex-UI WARP candidate runs
against the original adjacent baseline. The first comparison flagged dirty composition time and
process memory; the repeat was within the existing investigation bands. A subsequent governed test
invocation again flagged process memory. These mixed results leave performance acceptance OPEN;
the successful repeat does not erase earlier findings or waive the paired resource gate.

The full logs are under `C:/RedSalamander.Perf/evidence/i26-ui/dxui-localized-*`. Common receipts
retain executable, source-input, hardware/configuration identities and all five rounds. Further
tests add edge-case, render and gallery coverage; this first candidate is not the final handoff.

The expanded x64 Release run passed Control, Rendering and Embedded with zero capability skips;
their complete logs and suite receipts are retained here. `expanded-tests-performance.json` is that
run's unpaired common benchmark. Candidate fixture SHA-256:
`F14E4E4DAFD54DF43B5207290DF160C2117D94D04DBA4B9F0DF99D75CDF285AD`.
Removing whitespace from baseline/candidate fixture sources produces equal text; only clang-format
changed the input file. The fixture constants, measurement operations and assertions are unchanged.
