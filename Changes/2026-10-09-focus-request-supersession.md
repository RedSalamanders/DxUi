- 2026-10-09: Preserve callback-selected logical focus across superseded outer requests and TSF context retirement.
  Contain standard exceptions from focus/reset callbacks, acknowledge failed transitions safely, and retire old-tree
  focus before releasing its owner. Cover loss, gain, host notification, target retirement and the real TSF parent setter.
