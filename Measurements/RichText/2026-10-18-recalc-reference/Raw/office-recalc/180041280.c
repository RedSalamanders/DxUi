
/* public: virtual bool __cdecl CDisplayML::RecalcView(bool,struct CViewRect const & __ptr64,struct
   RECTUV * __ptr64) __ptr64 */

bool __thiscall
CDisplayML::RecalcView(CDisplayML *this,bool param_1,CViewRect *param_2,RECTUV *param_3)

{
  int iVar1;
  longlong *plVar2;
  CDisplayEnginePTS *pCVar3;
  undefined8 *puVar4;
  CDisplayEngineML *this_00;
  CArrayBase *this_01;
  code *pcVar5;
  undefined1 auVar6 [16];
  undefined1 auVar7 [16];
  undefined4 uVar8;
  undefined4 uVar9;
  undefined4 uVar10;
  undefined8 uVar11;
  bool bVar12;
  char cVar13;
  long lVar14;
  long lVar15;
  long lVar16;
  int iVar17;
  uint uVar18;
  undefined8 *puVar19;
  CLine *pCVar20;
  SafeInt<unsigned_int,safeint_exception_handlers::SafeInt_InvalidParameter> *this_02;
  ushort *puVar21;
  longlong lVar22;
  longlong lVar23;
  longlong lVar24;
  longlong lVar25;
  longlong lVar26;
  longlong lVar27;
  int iStackX_8;
  SafeInt<long,safeint_exception_handlers::SafeInt_InvalidParameter> aSStack_a4 [4];
  longlong lStack_a0;
  CLinePtr aCStack_98 [12];
  int iStack_8c;
  
  plVar2 = *(longlong **)(this + 0xb0);
  if (plVar2 == (longlong *)0x0) {
    return false;
  }
  lVar22 = plVar2[1];
  if (*(code **)(*plVar2 + 0x40) == CDisplayEnginePTS::GetDvi) {
    lVar14 = CFSREContext::GetDvi((CFSREContext *)plVar2[4]);
    lVar24 = (longlong)lVar14;
  }
  else if (*(code **)(*plVar2 + 0x40) == CDisplayEngineML::GetDvi) {
    lVar24 = plVar2[0xd];
  }
  else {
    lVar24 = (*(code *)__guard_dispatch_icall_fptr)();
  }
  lVar25 = *(longlong *)(lVar22 + 0x10);
  if (((*(uint *)(lVar25 + 0xd0) & 0x400000) != 0) || ((*(byte *)(lVar25 + 0x134) & 0xc0) == 0xc0))
  {
    if (lVar24 == 0) {
      lVar24 = 0;
    }
    else {
      lVar25 = (longlong)*(int *)(lVar22 + 0x70);
      lVar22 = -lVar24;
      if (-lVar24 < 0) {
        lVar22 = lVar24;
      }
      auVar6._8_8_ = 0;
      auVar6._0_8_ = SUB168(SEXT816(0x7fffffffffffffff),8);
      lVar26 = -lVar25;
      if (-lVar25 < 0) {
        lVar26 = lVar25;
      }
      if (lVar26 < SUB168((auVar6 << 0x40 | ZEXT816(0x7fffffffffffffff)) / SEXT816(lVar22),0)) {
        lVar22 = 0x6f9f0;
        if (lVar24 * lVar25 < 0) {
          lVar22 = -0x6f9f0;
        }
        lVar24 = (lVar22 + lVar24 * lVar25) / 0xdf3e0;
      }
      else {
        lVar24 = -1;
      }
    }
  }
  lVar14 = GetMaxVpScroll(this);
  pCVar3 = *(CDisplayEnginePTS **)(this + 0xb0);
  lVar22 = *(longlong *)(pCVar3 + 8);
  if (*(code **)(*(longlong *)pCVar3 + 0x38) == CDisplayEngineML::GetDuiLineMax) {
    iStackX_8 = *(int *)(pCVar3 + 0x60);
  }
  else if (*(code **)(*(longlong *)pCVar3 + 0x38) == CDisplayEnginePTS::GetDuiLineMax) {
    iStackX_8 = CDisplayEnginePTS::GetDuiLineMax(pCVar3);
  }
  else {
    iStackX_8 = (*(code *)__guard_dispatch_icall_fptr)();
  }
  lVar25 = *(longlong *)(lVar22 + 0x10);
  if (((*(uint *)(lVar25 + 0xd0) & 0x400000) != 0) || ((*(byte *)(lVar25 + 0x134) & 0xc0) == 0xc0))
  {
    lVar25 = -0x6f9f0;
    lVar22 = (longlong)*(int *)(lVar22 + 0x6c) * (longlong)iStackX_8;
    if (-1 < lVar22) {
      lVar25 = 0x6f9f0;
    }
    lVar22 = (lVar25 + lVar22) / 0xdf3e0;
    iStackX_8 = -1;
    if (lVar22 + 0x80000000U < 0x100000000) {
      iStackX_8 = (int)lVar22;
    }
  }
  puVar4 = *(undefined8 **)(this + 0xb8);
  if (puVar4 != (undefined8 *)0x0) {
    *(undefined4 *)((longlong)puVar4 + 0x44) = 0xffffffff;
    puVar4[7] = 0;
    *(undefined4 *)(puVar4 + 8) = 0;
    puVar19 = (undefined8 *)CLinePtr::CLinePtr(aCStack_98,(CDisplay *)this,(CLayout *)0x0);
    uVar11 = puVar19[1];
    *puVar4 = *puVar19;
    puVar4[1] = uVar11;
    uVar11 = puVar19[3];
    puVar4[2] = puVar19[2];
    puVar4[3] = uVar11;
    uVar8 = *(undefined4 *)((longlong)puVar19 + 0x24);
    uVar9 = *(undefined4 *)(puVar19 + 5);
    uVar10 = *(undefined4 *)((longlong)puVar19 + 0x2c);
    *(undefined4 *)(puVar4 + 4) = *(undefined4 *)(puVar19 + 4);
    *(undefined4 *)((longlong)puVar4 + 0x24) = uVar8;
    *(undefined4 *)(puVar4 + 5) = uVar9;
    *(undefined4 *)((longlong)puVar4 + 0x2c) = uVar10;
    puVar4[6] = puVar19[6];
  }
  if (*(longlong *)(this + 0x48) == 0) {
    return false;
  }
  this_00 = *(CDisplayEngineML **)(this + 0xb0);
  if (*(code **)(*(longlong *)this_00 + 0xb0) == CDisplayEngineML::RecalcView) {
    CRchTxtPtr::CRchTxtPtr
              ((CRchTxtPtr *)aCStack_98,0,*(CTxtStory **)(*(longlong *)(this_00 + 8) + 0x48));
    bVar12 = CDisplayEngineML::RecalcLines(this_00,(CRchTxtPtr *)aCStack_98,false);
    if (bVar12) {
      cVar13 = '\x01';
    }
    else {
      lVar22 = *(longlong *)(this_00 + 8);
      cVar13 = '\0';
      *(undefined4 *)(lVar22 + 0x88) = 0;
      *(undefined8 *)(lVar22 + 0xc0) = 0;
      *(undefined8 *)(lVar22 + 200) = 0;
      *(undefined4 *)(lVar22 + 0x98) = 0;
    }
  }
  else if (*(code **)(*(longlong *)this_00 + 0xb0) == CDisplayEnginePTS::RecalcView) {
    cVar13 = CDisplayEnginePTS::RecalcView((CDisplayEnginePTS *)this_00,param_2);
  }
  else {
    cVar13 = (*(code *)__guard_dispatch_icall_fptr)();
  }
  if (cVar13 == '\0') goto LAB_1800416d9;
  lVar22 = *(longlong *)(this + 0x50);
  if ((lVar22 != 0) && (*(longlong *)(*(longlong *)(this + 0x10) + 0x90) != 0)) {
    *(undefined8 *)(lVar22 + 4) = 0x3fffffff;
    *(undefined4 *)(lVar22 + 0xc) = 0;
  }
  if (*(code **)(**(longlong **)(this + 0xb0) + 0x38) == CDisplayEngineML::GetDuiLineMax) {
    iVar17 = (int)(*(longlong **)(this + 0xb0))[0xc];
  }
  else {
    iVar17 = (*(code *)__guard_dispatch_icall_fptr)();
  }
  if (iVar17 <= *(int *)(this + 0x78)) {
    *(undefined4 *)(this + 0x88) = 0;
  }
  lVar15 = GetMaxVpScroll(this);
  iVar17 = *(int *)(this + 0x98);
  iVar1 = *(int *)(this + 0xc4);
  lVar16 = 0;
  if (*(int *)(this + 0x7c) < lVar15) {
    lVar16 = iVar17;
  }
  *(undefined8 *)(this + 0xc4) = 0;
  *(undefined4 *)(this + 0x98) = 0;
  *(undefined4 *)(this + 0xc0) = 0;
  if (*(longlong *)(this + 0x48) != 0) {
    CLinePtr::CLinePtr(aCStack_98,(CDisplay *)this,(CLayout *)0x0);
    bVar12 = CLinePtr::SetCp(aCStack_98,lVar16,false,0);
    if ((!bVar12) && (bVar12 = CLinePtr::IsValid(aCStack_98), bVar12)) {
      lVar16 = CRunPtrBase::CalculateCp((CRunPtrBase *)aCStack_98);
      CLinePtr::SetCp(aCStack_98,lVar16,false,0);
    }
    pCVar3 = *(CDisplayEnginePTS **)(this + 0xb0);
    if (*(code **)(*(longlong *)pCVar3 + 0xd8) == CDisplayEngineML::VposFromLinePtr) {
      lVar15 = CLinePtr::GetLineIndex(aCStack_98);
      lVar15 = CLayoutLineArray::VposFromLine
                         ((CLayoutLineArray *)(pCVar3 + 0x18),*(CDisplayML **)(pCVar3 + 8),lVar15,
                          false);
    }
    else if (*(code **)(*(longlong *)pCVar3 + 0xd8) == CDisplayEnginePTS::VposFromLinePtr) {
      lVar15 = CDisplayEnginePTS::VposFromLinePtr(pCVar3,aCStack_98);
    }
    else {
      lVar15 = (*(code *)__guard_dispatch_icall_fptr)();
    }
    *(long *)(this + 0x98) = lVar16 - iStack_8c;
    *(long *)(this + 0xc0) = lVar15;
    lVar16 = CLinePtr::GetLineIndex(aCStack_98);
    *(long *)(this + 200) = lVar16;
    Sync_yScroll(this);
    if (*(int *)(this + 0x98) == iVar17) {
      this_01 = *(CArrayBase **)(this + 0xb0);
      lVar16 = *(long *)(this + 200);
      pcVar5 = *(code **)(*(longlong *)this_01 + 0x68);
      if (pcVar5 == CDisplayEngineML::GetLine) {
        pCVar20 = CArrayBase::Elem(this_01 + 0x28,lVar16);
      }
      else if (pcVar5 == CDisplayEnginePTS::GetLine) {
        pCVar20 = CDisplayEnginePTS::GetLine((CDisplayEnginePTS *)this_01,lVar16);
      }
      else if (pcVar5 == CArrayBase::Elem) {
        pCVar20 = CArrayBase::Elem(this_01,lVar16);
      }
      else {
        pCVar20 = (CLine *)(*(code *)__guard_dispatch_icall_fptr)();
      }
      if (pCVar20 != (CLine *)0x0) {
        if (((byte)pCVar20[0xd] & 0x10) == 0) {
          if (((byte)pCVar20[0xf] & 3) == 1) {
            if ((*(longlong *)(pCVar20 + 0x18) == 0) || ((char)pCVar20[0xe] < '\0'))
            goto LAB_180041a32;
            lStack_a0 = (longlong)*(int *)(*(longlong *)(pCVar20 + 0x18) + 8);
            this_02 = (SafeInt<unsigned_int,safeint_exception_handlers::SafeInt_InvalidParameter> *)
                      SafeInt<long,safeint_exception_handlers::SafeInt_InvalidParameter>::
                      SafeInt<long,safeint_exception_handlers::SafeInt_InvalidParameter><__int64>
                                (aSStack_a4,&lStack_a0);
            uVar18 = SafeInt<unsigned_int,safeint_exception_handlers::SafeInt_InvalidParameter>::
                     operator_unsigned_int(this_02);
          }
          else if (((byte)pCVar20[0xf] & 3) == 2) {
            if (*(longlong *)(pCVar20 + 0x18) == 0) goto LAB_180041a32;
            uVar18 = *(uint *)(*(longlong *)(pCVar20 + 0x18) + 0x10);
          }
          else {
            uVar18 = *(uint *)(pCVar20 + 0x1c);
          }
        }
        else {
LAB_180041a32:
          uVar18 = 0;
        }
        if (-iVar1 < (int)uVar18) {
          *(int *)(this + 0xc0) = *(int *)(this + 0xc0) - iVar1;
          *(int *)(this + 0xc4) = iVar1;
        }
      }
    }
  }
  lVar25 = -0x6f9f0;
  plVar2 = *(longlong **)(this + 0xb0);
  lVar22 = plVar2[1];
  if (*(code **)(*plVar2 + 0x40) == CDisplayEnginePTS::GetDvi) {
    lVar16 = CFSREContext::GetDvi((CFSREContext *)plVar2[4]);
    lVar26 = (longlong)lVar16;
  }
  else if (*(code **)(*plVar2 + 0x40) == CDisplayEngineML::GetDvi) {
    lVar26 = plVar2[0xd];
  }
  else {
    lVar26 = (*(code *)__guard_dispatch_icall_fptr)();
  }
  lVar27 = *(longlong *)(lVar22 + 0x10);
  if ((((*(uint *)(lVar27 + 0xd0) & 0x400000) != 0) || ((*(byte *)(lVar27 + 0x134) & 0xc0) == 0xc0))
     && (lVar26 != 0)) {
    lVar27 = (longlong)*(int *)(lVar22 + 0x70);
    lVar22 = -lVar26;
    if (-lVar26 < 0) {
      lVar22 = lVar26;
    }
    auVar7._8_8_ = 0;
    auVar7._0_8_ = SUB168(SEXT816(0x7fffffffffffffff),8);
    lVar23 = -lVar27;
    if (-lVar27 < 0) {
      lVar23 = lVar27;
    }
    if (lVar23 < SUB168((auVar7 << 0x40 | ZEXT816(0x7fffffffffffffff)) / SEXT816(lVar22),0)) {
      lVar22 = 0x6f9f0;
      if (lVar27 * lVar26 < 0) {
        lVar22 = -0x6f9f0;
      }
      lVar26 = (lVar22 + lVar27 * lVar26) / 0xdf3e0;
    }
    else {
      lVar26 = -1;
    }
  }
  if (((int)lVar24 == (int)lVar26) && (lVar16 = GetMaxVpScroll(this), lVar14 == lVar16)) {
    pCVar3 = *(CDisplayEnginePTS **)(this + 0xb0);
    lVar22 = *(longlong *)(pCVar3 + 8);
    if (*(code **)(*(longlong *)pCVar3 + 0x38) == CDisplayEngineML::GetDuiLineMax) {
      iVar17 = *(int *)(pCVar3 + 0x60);
    }
    else if (*(code **)(*(longlong *)pCVar3 + 0x38) == CDisplayEnginePTS::GetDuiLineMax) {
      iVar17 = CDisplayEnginePTS::GetDuiLineMax(pCVar3);
    }
    else {
      iVar17 = (*(code *)__guard_dispatch_icall_fptr)();
    }
    lVar24 = *(longlong *)(lVar22 + 0x10);
    if (((*(uint *)(lVar24 + 0xd0) & 0x400000) != 0) || ((*(byte *)(lVar24 + 0x134) & 0xc0) == 0xc0)
       ) {
      lVar22 = (longlong)*(int *)(lVar22 + 0x6c) * (longlong)iVar17;
      if (-1 < lVar22) {
        lVar25 = 0x6f9f0;
      }
      lVar22 = (lVar25 + lVar22) / 0xdf3e0;
      iVar17 = -1;
      if (lVar22 + 0x80000000U < 0x100000000) {
        iVar17 = (int)lVar22;
      }
    }
    if (iStackX_8 == iVar17) goto LAB_1800416d9;
  }
  lVar14 = CDisplay::RequestResize((CDisplay *)this);
  if (lVar14 < 0) {
    lVar22 = *(longlong *)(this + 0x10);
    lVar25 = 0;
    lVar24 = lVar22;
    if (*(short *)(lVar22 + 0x38) == 0) {
      lVar24 = lVar25;
    }
    lVar26 = lVar24 + 0x30;
    if (lVar24 == 0) {
      lVar26 = lVar25;
    }
    if (lVar26 != 0) {
      if (*(short *)(lVar22 + 0x38) == 0) {
        lVar22 = lVar25;
      }
      puVar21 = (ushort *)(lVar22 + 0x3a);
      if (lVar22 == 0) {
        puVar21 = (ushort *)&DAT_0000000a;
      }
      *puVar21 = *puVar21 | 0x40;
    }
  }
  else if ((param_3 != (RECTUV *)0x0) &&
          ((*(uint *)(*(CTxtEdit **)(this + 0x10) + 200) & 0x8000000) != 0)) {
    CTxtEdit::TxGetClientRect(*(CTxtEdit **)(this + 0x10),param_3,(RECTUV *)0x0);
  }
LAB_1800416d9:
  if (((param_1) && (*(longlong *)(this + 0x48) != 0)) && ((*(uint *)(this + 0x74) >> 8 & 1) != 0))
  {
    *(uint *)(this + 0x74) = *(uint *)(this + 0x74) & 0xfffffeff;
    if (*(code **)(*(longlong *)this + 0x28) == UpdateScrollBar) {
      UpdateScrollBar(this,1,true);
    }
    else {
      (*(code *)__guard_dispatch_icall_fptr)();
    }
    iVar17 = *(int *)(this + 0x7c);
    if (*(code **)(*(longlong *)this + 0x28) == UpdateScrollBar) {
      UpdateScrollBar(this,0,true);
    }
    else {
      (*(code *)__guard_dispatch_icall_fptr)();
    }
    if (*(int *)(this + 0x7c) < iVar17) {
      (*(code *)__guard_dispatch_icall_fptr)(this,1,1);
    }
  }
  lVar22 = *(longlong *)(this + 0x10);
  if ((((*(byte *)(lVar22 + 200) & 8) != 0) && (*(longlong *)(lVar22 + 0x150) != 0)) &&
     (*(CMsgCallBack **)(lVar22 + 0x158) != (CMsgCallBack *)0x0)) {
    CMsgCallBack::NotifyEvents(*(CMsgCallBack **)(lVar22 + 0x158),0x10);
  }
  return (bool)cVar13;
}

