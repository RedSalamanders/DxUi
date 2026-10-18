
/* public: virtual bool __cdecl CDisplayML::WaitForRecalcIli(long)const __ptr64 */

bool __thiscall CDisplayML::WaitForRecalcIli(CDisplayML *this,long param_1)

{
  bool bVar1;
  char cVar2;
  int iVar3;
  
  if (*(longlong *)(this + 0x48) == 0) {
    bVar1 = true;
  }
  else {
    iVar3 = (*(code *)__guard_dispatch_icall_fptr)();
    do {
      if ((((byte)this[0x74] & 0x80) != 0) || (param_1 < iVar3)) {
        bVar1 = param_1 < iVar3;
        goto LAB_18014be68;
      }
      bVar1 = CDisplay::FLayoutRecalcPending((CDisplay *)this);
    } while ((!bVar1) && (cVar2 = (*(code *)__guard_dispatch_icall_fptr)(), cVar2 != '\0'));
    bVar1 = false;
  }
LAB_18014be68:
  return bVar1;
}

