# Descendant timeout fixture admission

[Qualification](qualification.json) records 168 passing profiling cases and a focused
same-fixture counterfactual. With a deliberate 3.2-second parent setup, the old two-second
fixture deadline expires before holder identity can be published. A twelve-second fixture
deadline admits that setup, then proves that the actual runner terminates the original
parent while the identified holder remains alive and its pipe drain stays bounded. The
exact holder subsequently exits cooperatively.

The production measurement deadline remains 120 seconds; the post-exit drain remains
0.2 seconds in this fixture. The production runner is unchanged.

The original editor-cohort validation passed 167 of 168 profiling cases and found no
holder identity after its two-second deadline. Its complete output is retained. That
first fixture did not retain child diagnostics, so its exact cause remains unclassified.
The deterministic counterfactual establishes the startup-admission flaw without assigning
an unsupported cause to that historical failure. The initial private AST extraction
error is retained separately and is not counted as a timeout witness.
