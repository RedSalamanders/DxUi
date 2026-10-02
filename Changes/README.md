# Changelog fragments

Record a change here, not in [CHANGELOG.md](../CHANGELOG.md). Add one file named `<yyyy-mm-dd>-<topic>.md`, the topic in
lowercase letters, digits and hyphens. It holds one Markdown bullet written as a changelog entry, with every
continuation line and sub-bullet indented. Two branches that each prepend an entry to `CHANGELOG.md` conflict, so they
merge one after another and each runs CI again. Two new files never conflict, so changes merge in any order.

After a batch of merges, `Tools/Fold-Changelog.ps1` moves the fragments under the changelog's Unreleased heading,
newest first, and removes them. Run it in a change of its own. `validate-specs.ps1` checks every fragment's name and
shape.
