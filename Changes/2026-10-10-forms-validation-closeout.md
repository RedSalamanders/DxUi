- Retain S0-S2 intrinsic/Flex/form validation across all six builds and all 57 x64 noninteractive suite/log pairs,
  plus twelve successful Debug/Release/ASan foreground suite/log pairs, exact desktop restoration, candidate-only
  Manual/Form diagnostics and bounded Grid heap/retention runs. Preserve first failures and confirmation timeouts,
  including ASan's third attempt with no child, failed restoration and a subsequent unavailable-input-desktop
  check. The fourth locally confirmed ASan run completes the local x64 focus gate; native ARM64, physical-input,
  consumer and paired-policy acceptance remain separately open.
