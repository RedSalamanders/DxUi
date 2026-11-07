- Retire the module-owned animation window when its last native host detaches, before failure-host uninstallation
  and thread-local destruction. Preserve sibling subscriptions and the warm cache while hosts remain attached,
  and defer destruction during a timer callback. Add public-consumer shutdown and callback-retirement regressions
  and document the native host and failure-owner lifetime contracts.
