
/* public: virtual void __cdecl CDisplayML::StepBackgroundRecalc(void) __ptr64 */

void __thiscall CDisplayML::StepBackgroundRecalc(CDisplayML *this)

{
  uint uVar1;
  char cVar2;
  CWriteLock aCStackX_8 [8];
  
  CWriteLock::CWriteLock(aCStackX_8,0);
  if ((((byte)this[0x74] & 1) != 0) && (*(longlong *)(this + 0x48) != 0)) {
    cVar2 = (*(code *)__guard_dispatch_icall_fptr)();
    uVar1 = *(uint *)(this + 0x74);
    if (((uVar1 & 8) == 0) && (((byte)this[0xd0] & 2) == 0)) {
      *(uint *)(this + 0x74) = uVar1 | 8;
      if ((uVar1 >> 0xc & 1) == 0) {
        *(uint *)(this + 0x74) = uVar1 | 0x48;
        cVar2 = '\x01';
      }
      if (cVar2 == '\0') {
        (*(code *)__guard_dispatch_icall_fptr)();
        *(uint *)(this + 0x74) = *(uint *)(this + 0x74) & 0xfffffff7;
      }
      else {
        CTxtEdit::TxKillTimer(*(CTxtEdit **)(this + 0x10),0x1af);
        *(uint *)(this + 0x74) = *(uint *)(this + 0x74) & 0xfffffff6 | 0x80;
      }
    }
  }
  CWriteLock::~CWriteLock(aCStackX_8);
  return;
}

