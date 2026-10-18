
/* public: bool __cdecl CDisplay::FLayoutRecalcPending(void)const __ptr64 */

bool __thiscall CDisplay::FLayoutRecalcPending(CDisplay *this)

{
  if ((*(longlong *)(this + 0x50) == 0) ||
     ((((byte)this[0x74] & 0x40) == 0 && ((*(byte *)(*(longlong *)(this + 0x50) + 0x10) & 8) == 0)))
     ) {
    return false;
  }
  return true;
}

