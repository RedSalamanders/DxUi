# Configured icon fallback reading direction

## Checklist

- [x] Forward configured reading direction through the missing-Fluent Small fallback.
- [x] Add deterministic coverage for Icon, IconLarge and HeroIcon with a seeded unavailable-family cache entry; verify Small RTL/LTR separation and warm reuse.
- [x] Record the fallback behavior in the typography contract and hosting documentation.
- [ ] Run the focused WindowHost regression and required x64 Debug, Release, ASan Debug, and ARM64 build profiles after compiler coordination.
- [ ] Complete the six-profile source validation and report any remaining platform/runtime qualification separately.

The test seed uses the existing `Typography` font-family cache under its mutex, targets only the retained owner's exact factory/family key, and restores that entry's prior answer. No public API or font installation is required. Root reviewed the revised deterministic test; the caption performance trial is closed. Build/native execution will use one coordinated source revision with the separate Menu test repair after its final review.
