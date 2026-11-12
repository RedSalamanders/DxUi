- Service synchronous setup callbacks while accessibility workers publish readiness, retaining the original
  500 ms startup, 50 ms pending and two-second dispatch assertions. Retire the RangeFromPoint worker before
  reporting its captured observations, and cover withheld readiness with the real provider operation.
