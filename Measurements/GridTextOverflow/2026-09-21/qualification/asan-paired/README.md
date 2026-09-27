# V11 matched ASan measurements

Original production (`c52a8f5`) and V11 (`725e83c`, production `71d6446`) use identical
current benchmark inputs. The original ASan executable was rebuilt first; the
candidate is the exact executable that passed final functional qualification.
Each report records source, executable, workload and benchmark fingerprints.
The eight report/comparison files and three build/measurement log families are
unchanged copies from `C:/RedSalamander.Perf/evidence/i26-ui`.

The default fixture is within the existing noise budget: dirty FPS 98.229 -> 96.767,
private bytes 532,676,608 -> 534,249,472. Multiline dirty FPS is 26.639 -> 50.089,
private bytes 446,578,688 -> 450,580,480, working set 396,894,208 -> 380,301,312.
Surface storage remains 3,686,400 bytes in both fixtures. The multiline comparison
still requires advice because its clean-frame memory/timing metrics exceed bands;
retain every flag and do not treat the faster dirty path as overall acceptance.

ASan allocator overhead makes these configuration-specific measurements. They do
not waive the Release memory regression or establish a settled retention bound.
The earlier ASan receipts remain historical/unpaired; these are the current matched
pairs, not replacement baselines selected from earlier attempts.
