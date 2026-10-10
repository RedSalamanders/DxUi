- 2026-10-09: Stage application text-service activation and retire old resources before client or TSF callbacks.
  Preserve a callback-installed successor and foreign TSF focus, reject stale deferred store generations, and
  refuse reentrant attachment during Detach while allowing explicit reattachment after teardown completes.
