# Tool inventory

Root PowerShell entrypoints are the stable developer interface. Python validation is repository-owned and uses
the pinned dependency in requirements-validation.txt. `DXUI_PYTHON` may select an explicit interpreter.

| Tool | Purpose |
| --- | --- |
| Invoke-Python.ps1 | Interpreter selection and error propagation |
| validate_skills.py | Skill front matter and required instructions |
| validate_specs.py | Authority, local links and active-plan indexing |
| validate_dependencies.py | Exact import hashes, dependency pin and independent active includes |
| validate_consumer.ps1 | Exact consumer revision/API/target lock checks |
| tests/test_validation.py | Good and corrupted import/metadata regression cases |

Build/test receipts and scratch belong under `.build`. No tool uploads test data or changes audio/camera state.
Do not introduce a personal Codex path or silently install dependencies as part of validation.
