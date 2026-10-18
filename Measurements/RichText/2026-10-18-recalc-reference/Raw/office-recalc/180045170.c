
/* public: void __cdecl CDisplay::InvalidateRecalc(bool) __ptr64 */

void __thiscall CDisplay::InvalidateRecalc(CDisplay *this,bool param_1)

{
  longlong lVar1;
  CDisplay *pCVar2;
  int iVar3;
  
  *(uint *)(this + 0x74) = *(uint *)(this + 0x74) | 0x40;
  if (param_1) {
    if (*(code **)(*(longlong *)this + 0x48) == CDisplayEnginePTS::ClearContexts) {
      if (*(CFSREContext **)(this + 0x20) != (CFSREContext *)0x0) {
        CFSREContext::Clear(*(CFSREContext **)(this + 0x20),true);
      }
      if (*(CFSREContext **)(this + 0x28) != (CFSREContext *)0x0) {
        CFSREContext::Clear(*(CFSREContext **)(this + 0x28),true);
      }
    }
    else {
      pCVar2 = this;
      if (*(code **)(*(longlong *)this + 0x48) == CDisplayML::ClearLayoutCaches) {
        pCVar2 = *(CDisplay **)(this + 0xb0);
      }
      (*(code *)__guard_dispatch_icall_fptr)(pCVar2);
    }
    lVar1 = *(longlong *)(this + 0x50);
    if ((lVar1 != 0) && (*(longlong *)(this + 0x48) != 0)) {
      iVar3 = *(int *)(*(longlong *)(this + 0x48) + 0x28);
      *(byte *)(lVar1 + 0x10) = *(byte *)(lVar1 + 0x10) | 8;
      if (*(int *)(lVar1 + 4) == 0x3fffffff) {
        *(undefined4 *)(lVar1 + 4) = 0;
        *(undefined4 *)(lVar1 + 0xc) = 0;
      }
      else {
        if (0 < *(int *)(lVar1 + 4)) {
          *(undefined4 *)(lVar1 + 4) = 0;
        }
        if (iVar3 <= *(int *)(lVar1 + 8) + *(int *)(lVar1 + 0xc)) {
          return;
        }
        iVar3 = iVar3 - *(int *)(lVar1 + 0xc);
      }
      *(int *)(lVar1 + 8) = iVar3;
    }
  }
  return;
}

