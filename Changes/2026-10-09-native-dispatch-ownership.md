- Harden native attachment and accessibility dispatch: reject competing or repeated attachments that would
  leave stale owners, require exact HWND/message/type when taking bounded payloads, and propagate failed menu
  posts through a compatible HRESULT accessible-invoke callback. Exhaustion and retry, provider retirement,
  callback failure, and rejected reattachment have registered regression coverage.
