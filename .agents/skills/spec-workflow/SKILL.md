---
name: spec-workflow
description: Create or reconcile DxUi normative contracts and indexed WIP/Done plans.
---

# Spec workflow

Read [the owning contract](../../../Specs/README.md) and [AGENTS.md](../../../AGENTS.md).

Read Specs/README.md. State implemented support separately from intended acceptance contracts. Plans own sequencing, not duplicate durable requirements. Validate links and indexes. Close a plan only when implementation, tests and normative updates are complete; leave later consumer migration on HOLD.

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; library tests do not prove pending RedXe bridges or later RedSalamander migration.
