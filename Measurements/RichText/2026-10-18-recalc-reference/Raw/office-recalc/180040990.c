
/* protected: virtual bool __cdecl CDisplayEngineML::WaitForRecalc(long,long) __ptr64 */

bool __thiscall CDisplayEngineML::WaitForRecalc(CDisplayEngineML *this,long param_1,long param_2)

{
  longlong lVar1;
  CTxtEdit *this_00;
  longlong lVar2;
  CDisplayML *this_01;
  bool bVar3;
  HCURSOR pHVar4;
  HICON__ *pHVar5;
  int iVar6;
  int iVar7;
  undefined1 uVar8;
  CRchTxtPtr aCStack_68 [96];
  
  uVar8 = (undefined1)param_2;
  lVar1 = *(longlong *)(this + 8);
  if (lVar1 == 0) {
    return false;
  }
  this_00 = *(CTxtEdit **)(this + 0x10);
  bVar3 = true;
  if (*(longlong *)(this_00 + 600) != 0) {
    lVar2 = *(longlong *)(*(longlong *)(this_00 + 600) + 0x18);
    if (lVar2 == 0) {
      iVar7 = 0;
    }
    else {
      iVar7 = *(int *)(lVar2 + 0x20);
    }
    if (iVar7 < *(int *)(this + 0x70)) {
      *(undefined8 *)(this + 0x70) = 0;
    }
  }
  if (((param_2 < 0) || (*(int *)(this + 0x74) <= param_2)) &&
     ((param_1 < 0 || (*(int *)(this + 0x70) <= param_1)))) {
    iVar7 = 0;
    if (*(longlong *)(lVar1 + 0x48) != 0) {
      iVar7 = *(int *)(*(longlong *)(lVar1 + 0x48) + 0x28);
    }
    iVar6 = *(int *)(this + 0x70);
    iVar7 = iVar7 - iVar6;
    if ((iVar7 < 1) && (*(int *)(this + 0x30) != 0)) {
      if (iVar7 == 0) {
        *(uint *)(lVar1 + 0x74) = *(uint *)(lVar1 + 0x74) | 0x80;
      }
    }
    else {
      *(long *)(this + 0x78) = param_1;
      *(long *)(this + 0x7c) = param_2;
      if ((iVar7 < 0x1801) || ((*(uint *)(this_00 + 0x140) & 0x200) != 0)) {
        pHVar5 = (HICON__ *)0x0;
      }
      else {
        pHVar4 = LoadCursorW((HINSTANCE)0x0,(LPCWSTR)0x7f02);
        pHVar5 = CTxtEdit::TxSetCursor(this_00,pHVar4,(bool)uVar8);
        iVar6 = *(int *)(this + 0x70);
      }
      CRchTxtPtr::CRchTxtPtr(aCStack_68,iVar6,*(CTxtStory **)(*(longlong *)(this + 8) + 0x48));
      if ((*(int *)(this + 0x70) == 0) || ((*(byte *)(*(longlong *)(this + 8) + 0x74) & 0x40) != 0))
      {
        uVar8 = 1;
        bVar3 = RecalcLines(this,aCStack_68,true);
        CDisplayML::RebindFirstVisible(*(CDisplayML **)(this + 8),0);
        if (!bVar3) {
          lVar1 = *(longlong *)(this + 8);
          *(undefined4 *)(lVar1 + 0x88) = 0;
          *(undefined8 *)(lVar1 + 0xc0) = 0;
          *(undefined8 *)(lVar1 + 200) = 0;
          *(undefined4 *)(lVar1 + 0x98) = 0;
        }
      }
      else {
        uVar8 = (undefined1)iVar7;
        bVar3 = RecalcLines(this,aCStack_68,iVar7,iVar7,false,true,(CLed *)0x0);
      }
      if (0x1800 < iVar7) {
        CTxtEdit::TxSetCursor(*(CTxtEdit **)(this + 0x10),pHVar5,(bool)uVar8);
      }
    }
  }
  this_01 = *(CDisplayML **)(this + 8);
  if ((*(longlong *)(this_01 + 0x48) != 0) && ((*(uint *)(this_01 + 0x74) >> 8 & 1) != 0)) {
    *(uint *)(this_01 + 0x74) = *(uint *)(this_01 + 0x74) & 0xfffffeff;
    if (*(code **)(*(longlong *)this_01 + 0x28) == CDisplayML::UpdateScrollBar) {
      CDisplayML::UpdateScrollBar(this_01,1,true);
    }
    else {
      (*(code *)__guard_dispatch_icall_fptr)();
    }
    iVar7 = *(int *)(this_01 + 0x7c);
    if (*(code **)(*(longlong *)this_01 + 0x28) == CDisplayML::UpdateScrollBar) {
      CDisplayML::UpdateScrollBar(this_01,0,true);
    }
    else {
      (*(code *)__guard_dispatch_icall_fptr)();
    }
    if (*(int *)(this_01 + 0x7c) < iVar7) {
      (*(code *)__guard_dispatch_icall_fptr)(this_01,1,1);
    }
  }
  return bVar3;
}

