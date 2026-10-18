
/* protected: bool __cdecl CDisplayEngineML::StartBackgroundRecalc(void) __ptr64 */

bool __thiscall CDisplayEngineML::StartBackgroundRecalc(CDisplayEngineML *this)

{
  uint *puVar1;
  longlong lVar2;
  bool bVar3;
  int iVar4;
  undefined4 uVar5;
  
  if ((*(byte *)(*(longlong *)(this + 8) + 0x74) & 1) == 0) {
    lVar2 = *(longlong *)(*(longlong *)(this + 8) + 0x48);
    iVar4 = 0;
    if (lVar2 != 0) {
      iVar4 = *(int *)(lVar2 + 0x28);
    }
    if (*(int *)(this + 0x70) != iVar4) {
      iVar4 = CTxtEdit::TxSetTimer(*(CTxtEdit **)(this + 0x10),0x1af,300);
      if (iVar4 != 0) {
        puVar1 = (uint *)(*(longlong *)(this + 8) + 0x74);
        *puVar1 = *puVar1 & 0xffffff7f;
        *(uint *)(*(longlong *)(this + 8) + 0x74) = *(uint *)(*(longlong *)(this + 8) + 0x74) | 1;
        goto LAB_1801a53d2;
      }
      *(undefined4 *)(*(longlong *)(this + 8) + 0xc4) = 0;
      *(undefined4 *)(*(longlong *)(this + 8) + 0x98) = 0;
      *(undefined4 *)(*(longlong *)(this + 8) + 200) = 0;
      *(undefined4 *)(*(longlong *)(this + 8) + 0xc0) = 0;
      lVar2 = *(longlong *)(*(longlong *)(this + 8) + 0x48);
      uVar5 = 0;
      if (lVar2 != 0) {
        uVar5 = *(undefined4 *)(lVar2 + 0x28);
      }
      (*(code *)__guard_dispatch_icall_fptr)(*(longlong *)(this + 8),uVar5,0xffffffff);
    }
    bVar3 = true;
  }
  else {
LAB_1801a53d2:
    bVar3 = false;
  }
  return bVar3;
}

