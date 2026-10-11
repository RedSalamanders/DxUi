# Bounded focus setup diagnostics

The two `.source.receipt.txt` control files retain the exact corrected compiled test inputs. The public library,
activation order, timeout, retry policy and behavior assertions are unchanged. Each setup attempt resets its success
flag and records no more than four passive stages; the guarded sequence permits five attempts. The local passive
TextField subclass exposes its inherited lifetime token only in this fixture so added observations avoid a dangling
field dereference. `sequenceHeld` labels the complete guarded sequence, not each attempt.

The initial Debug build failed with protected-token access (C2248); its complete parent log remains failed evidence.
The corrected source and its independent root/agent source review are separate from that failure. Source review
and successful compilation alone do not qualify runtime behavior or desktop restoration. The corrected source
passes [all six builds](builds-final/README.md) and [57 noninteractive suite executions](../setup-runs/README.md),
with zero skips and actual post-native provenance. Two ASan confirmations expire before any child runs, with all
three desktop states verified unchanged. The third ASan attempt exercises these setup observations and passes
four full foreground suites, zero skips, with verified restoration. Fresh Release/Debug foreground validation
remains required for their own binaries; the earlier failures' cause remains unproved.

`snapshot-reviewer.source.receipt.txt` retains the private review helper as historical text. It preserves every raw
native log/receipt/result and reads complete actual post-focus identity even for a failed attempt. Its hard-coded
task paths make it unsuitable as a portable runner. `source-copy.receipt.txt` binds original paths, lengths and
SHA-256 values at capture time; future source edits cannot change these retained bytes.

The [foreground inventory](../README.md) keeps earlier canceled/failed/passing attempts, their scope and restoration
outcomes, and limitations separately. Sequential setup observations do not identify an intervening Windows message,
TSF HRESULT or scheduler cause. No performance acceptance or consumer/platform qualification follows.

API interpretation was checked against Microsoft's [SetFocus](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setfocus),
[SetActiveWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setactivewindow) and
[AttachThreadInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-attachthreadinput) contracts.
Thread active/focus state and foreground ownership are separate observations; attached queues share focus state until
detachment. These contracts explain the recorded fields, without establishing the earlier failure's cause.
