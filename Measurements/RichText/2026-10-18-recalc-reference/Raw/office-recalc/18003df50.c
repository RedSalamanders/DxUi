
/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */
/* protected: bool __cdecl CDisplayEngineML::RecalcLines(class CRchTxtPtr const & __ptr64,bool)
   __ptr64 */

bool __thiscall
CDisplayEngineML::RecalcLines(CDisplayEngineML *this,CRchTxtPtr *param_1,bool param_2)

{
  int *piVar1;
  CLine CVar2;
  long lVar3;
  longlong lVar4;
  undefined1 auVar5 [16];
  char cVar6;
  bool bVar7;
  byte bVar8;
  bool bVar9;
  long lVar10;
  int iVar11;
  long lVar12;
  long lVar13;
  uint uVar14;
  int iVar15;
  __int64 _Var16;
  CLine *pCVar17;
  ulonglong uVar18;
  ushort *puVar19;
  CParaFormat *pCVar20;
  int iVar21;
  longlong lVar22;
  uint uVar23;
  byte bVar24;
  longlong lVar25;
  CDisplay *this_00;
  uint uVar26;
  ulonglong uVar27;
  undefined1 auStackY_278 [32];
  bool bStack_228;
  byte bStack_227;
  bool bStack_226;
  byte bStack_224;
  long lStack_210;
  int iStack_20c;
  undefined8 uStack_1f0;
  undefined8 uStack_1e8;
  undefined8 uStack_1e0;
  undefined8 uStack_1d8;
  CMeasurer aCStack_1c8 [32];
  int iStack_1a8;
  byte bStack_8a;
  ulonglong uStack_48;
  
  uStack_48 = __security_cookie ^ (ulonglong)auStackY_278;
  lVar4 = *(longlong *)(this + 0x10);
  if (((*(uint *)(lVar4 + 0xd0) & 0x400000) != 0) ||
     (bStack_228 = false, (*(byte *)(lVar4 + 0x134) & 0xc0) == 0xc0)) {
    bStack_228 = true;
  }
  if ((*(longlong *)(this + 8) == 0) || (lVar4 == 0)) {
    return false;
  }
  uVar27 = 0;
  lVar12 = 0;
  uVar18 = 0;
  if (((*(longlong *)(lVar4 + 0x118) != 0) &&
      (lVar25 = *(longlong *)(*(longlong *)(lVar4 + 0x118) + 0x38), lVar25 != 0)) &&
     (lVar25 = *(longlong *)(lVar25 + 0x40), lVar25 != 0)) {
    piVar1 = (int *)(lVar25 + 0x10);
    *piVar1 = *piVar1 + 1;
  }
  lVar10 = CDisplayML::GetMaxVpScroll(*(CDisplayML **)(this + 8));
  if (((*(byte *)(lVar4 + 200) & 0x80) == 0) && (*(longlong *)(lVar4 + 0x118) != 0)) {
    bVar24 = *(byte *)(*(longlong *)(lVar4 + 0x118) + 0x99) & 3;
  }
  else {
    bVar24 = 0;
  }
  *(uint *)(*(longlong *)(this + 8) + 0x74) = *(uint *)(*(longlong *)(this + 8) + 0x74) & 0xffbfffff
  ;
  CLayoutLineArray::DeleteSubLayouts((CLayoutLineArray *)(this + 0x18),0,-1);
  CArrayBase::Remove((CArrayBase *)(this + 0x28),0,-1);
  *(undefined8 *)(this + 0x70) = 0;
  CMeasurer::CMeasurer(aCStack_1c8,*(CDisplay **)(this + 8),param_1);
  CRchTxtPtr::SetCp((CRchTxtPtr *)aCStack_1c8,0);
  this_00 = *(CDisplay **)(this + 8);
  if (*(longlong *)(this_00 + 0x48) == 0) {
    iVar15 = 0;
  }
  else {
    iVar15 = *(int *)(*(longlong *)(this_00 + 0x48) + 0x28);
  }
  iStack_20c = 0x3c;
  lVar3 = *(long *)(this_00 + 0x7c);
  _Var16 = CDisplay::DVtoIV(this_00,lVar3);
  bVar9 = true;
  uVar14 = 1;
  bStack_224 = 1;
  bStack_226 = param_2;
  if (((*(uint *)(lVar4 + 0x130) & 0x40000) == 0) && ((*(uint *)(this_00 + 0x74) >> 0xc & 1) != 0))
  {
    *(undefined8 *)(this + 0x78) = 0xffffffffffffffff;
    bStack_226 = true;
  }
  lStack_210 = -1;
  uVar26 = 0xffffffff;
  bStack_227 = 0;
  if (iStack_1a8 < iVar15) {
    do {
      pCVar17 = CArrayBase::ArAdd((CArrayBase *)(this + 0x28),1,(long *)0x0);
      iVar21 = (int)uVar27;
      if (pCVar17 == (CLine *)0x0) {
        lVar25 = 0;
        if (*(short *)(lVar4 + 0x38) != 0) {
          lVar25 = lVar4;
        }
        lVar22 = lVar25 + 0x30;
        if (lVar25 == 0) {
          lVar22 = 0;
        }
        if (lVar22 != 0) {
          lVar25 = 0;
          if (*(short *)(lVar4 + 0x38) != 0) {
            lVar25 = lVar4;
          }
          puVar19 = (ushort *)(lVar25 + 0x3a);
          if (lVar25 == 0) {
            puVar19 = (ushort *)&DAT_0000000a;
          }
          *puVar19 = *puVar19 | 0x40;
        }
        goto LAB_18003e503;
      }
      iVar11 = 0;
      if (*(int *)(this + 0x38) + -0x20 != 0) {
        iVar11 = (int)((ulonglong)(longlong)(*(int *)(this + 0x38) + -0x20) >> 2);
      }
      if (iVar11 == 3) {
        bVar8 = 0x20;
LAB_18003e162:
        pCVar17[0xf] = (CLine)((byte)pCVar17[0xf] & 0x1f | bVar8);
      }
      else {
        if (iVar11 == 5) {
          bVar8 = 0x40;
          goto LAB_18003e162;
        }
        if (iVar11 == 8) {
          bVar8 = 0x60;
          goto LAB_18003e162;
        }
        if (iVar11 == 0xc) {
          bVar8 = 0x80;
          goto LAB_18003e162;
        }
      }
      cVar6 = (*(code *)__guard_dispatch_icall_fptr)();
      iVar11 = iStack_1a8;
      uVar23 = 0x12;
      if (cVar6 != '\0') {
        uVar23 = 2;
      }
      uVar23 = uVar23 | uVar14;
      bVar7 = CLayoutLineArray::Measure
                        ((CLayoutLineArray *)(this + 0x18),aCStack_1c8,pCVar17,
                         *(int *)(this + 0x30) + -1,uVar23,(long *)0x0,0,(CLine *)0x0,(long *)0x0);
      if (!bVar7) {
LAB_18003e503:
        if ((*(byte *)(*(longlong *)(this + 8) + 0xd0) & 2) == 0) {
          *(int *)(this + 0x70) = iStack_1a8;
          *(int *)(this + 0x74) = iVar21;
          puVar19 = (ushort *)(*(longlong *)(this + 8) + 0xd0);
          *puVar19 = *puVar19 | 2;
          lVar25 = 0;
          if (*(short *)(lVar4 + 0x38) != 0) {
            lVar25 = lVar4;
          }
          lVar22 = lVar25 + 0x30;
          if (lVar25 == 0) {
            lVar22 = 0;
          }
          if (lVar22 != 0) {
            lVar25 = 0;
            if (*(short *)(lVar4 + 0x38) != 0) {
              lVar25 = lVar4;
            }
            puVar19 = (ushort *)(lVar25 + 0x3a);
            if (lVar25 == 0) {
              puVar19 = (ushort *)&DAT_0000000a;
            }
            *puVar19 = *puVar19 | 0x40;
          }
          *(ushort *)(*(longlong *)(this + 8) + 0xd0) =
               *(ushort *)(*(longlong *)(this + 8) + 0xd0) & 0xfffd;
        }
        CMeasurer::~CMeasurer(aCStack_1c8);
        return false;
      }
      if (bVar24 == 0) {
LAB_18003e324:
        cVar6 = '\0';
        lStack_210 = iVar11;
        uVar26 = *(int *)(this + 0x30) - 1;
        bStack_227 = bStack_224;
        lVar12 = CLine::GetHeight(pCVar17,bStack_228,*(CDisplay **)(this + 8));
        uVar18 = uVar18 + (longlong)lVar12;
        lVar25 = *(longlong *)(*(longlong *)(this + 8) + 0x10);
        if (((*(uint *)(lVar25 + 0xd0) & 0x400000) == 0) &&
           ((*(byte *)(lVar25 + 0x134) & 0xc0) != 0xc0)) {
          uVar27 = uVar18;
        }
        else if (uVar18 == 0) {
          uVar27 = 0;
        }
        else {
          lVar25 = (longlong)*(int *)(*(longlong *)(this + 8) + 0x70);
          uVar27 = -uVar18;
          if ((longlong)-uVar18 < 0) {
            uVar27 = uVar18;
          }
          auVar5._8_8_ = 0;
          auVar5._0_8_ = SUB168(SEXT816(0x7fffffffffffffff),8);
          lVar22 = -lVar25;
          if (-lVar25 < 0) {
            lVar22 = lVar25;
          }
          if (lVar22 < SUB168((auVar5 << 0x40 | ZEXT816(0x7fffffffffffffff)) /
                              SEXT816((longlong)uVar27),0)) {
            lVar22 = -0x6f9f0;
            if (-1 < (longlong)(lVar25 * uVar18)) {
              lVar22 = 0x6f9f0;
            }
            uVar27 = (longlong)(lVar22 + lVar25 * uVar18) / 0xdf3e0;
          }
          else {
            uVar27 = 0xffffffffffffffff;
          }
        }
        bVar8 = (byte)pCVar17[0xf] & 3;
        if (bVar8 != 1) {
          iVar21 = (int)uVar27 - iVar21;
          if (bVar8 == 0) {
            *(int *)(pCVar17 + 0x1c) = iVar21;
          }
          else if ((bVar8 == 2) && (*(longlong *)(pCVar17 + 0x18) != 0)) {
            *(int *)(*(longlong *)(pCVar17 + 0x18) + 0x10) = iVar21;
          }
        }
      }
      else {
        lVar12 = CLine::GetHeight(pCVar17,bStack_228,*(CDisplay **)(this + 8));
        lVar13 = CLine::GetDescent(pCVar17,bStack_228);
        if ((longlong)(((longlong)lVar12 - (longlong)lVar13) + uVar18) <= _Var16)
        goto LAB_18003e324;
        bStack_8a = bStack_8a | 0x80;
        if (uVar26 == 0xffffffff) {
          CRchTxtPtr::SetCp((CRchTxtPtr *)aCStack_1c8,iVar11);
          uVar26 = 0;
        }
        else {
          CRchTxtPtr::SetCp((CRchTxtPtr *)aCStack_1c8,lStack_210);
          if ((uVar26 < *(uint *)(this + 0x30)) && (*(longlong *)(this + 0x28) != 0)) {
            uVar14 = uVar26;
            if ((this[0x3c] != (CDisplayEngineML)0x0) && (*(int *)(this + 0x40) <= (int)uVar26)) {
              uVar14 = (*(int *)(this + 0x34) - *(uint *)(this + 0x30)) + uVar26;
            }
            pCVar17 = (CLine *)((longlong)(int)(uVar14 * *(int *)(this + 0x38)) +
                               *(longlong *)(this + 0x28));
          }
          else {
            pCVar17 = (CLine *)0x0;
          }
          CArrayBase::Remove((CArrayBase *)(this + 0x28),uVar26 + 1,1);
          uVar23 = (bStack_227 != 0) + 2;
        }
        bVar7 = CLayoutLineArray::Measure
                          ((CLayoutLineArray *)(this + 0x18),aCStack_1c8,pCVar17,uVar26,uVar23,
                           (long *)0x0,0,(CLine *)0x0,(long *)0x0);
        if (!bVar7) goto LAB_18003e503;
        bVar24 = 0;
        cVar6 = '\x01';
        uVar14 = *(uint *)(*(longlong *)(this + 8) + 0x74);
        *(uint *)(*(longlong *)(this + 8) + 0x74) =
             ((uint)(byte)pCVar17[0x10] << 0x16 | uVar14) & 0x400000 ^ uVar14 & 0xffbfffff;
        bStack_8a = bStack_8a & 0x7f;
        uVar27 = (ulonglong)(lVar3 + 1U);
        uVar18 = CDisplay::DVtoIV(*(CDisplay **)(this + 8),lVar3 + 1U);
      }
      lVar12 = (long)uVar27;
      CVar2 = pCVar17[0xd];
      *(int *)(this + 0x70) = iStack_1a8;
      if (bStack_226 != false) {
        if (bVar9) {
          if (*(int *)(*(longlong *)(this + 8) + 0x98) < iStack_1a8) {
            *(long *)(this + 0x7c) = lVar12 + lVar3;
            bVar9 = false;
          }
        }
        else if (*(int *)(this + 0x7c) < lVar12) {
          if (iStack_20c < 1) {
            bVar24 = 0;
            goto LAB_18003e5ae;
          }
          iStack_20c = iStack_20c + -1;
        }
      }
      if (cVar6 != '\0') break;
      bStack_224 = (byte)CVar2 >> 7;
      uVar14 = (uint)bStack_224;
    } while (iStack_1a8 < iVar15);
    bVar24 = 1;
  }
  else {
    bVar24 = 1;
  }
LAB_18003e5ae:
  if (*(int *)(this + 0x30) == 0) {
    pCVar17 = CLineArray::Add((CLineArray *)(this + 0x28),1,(long *)0x0);
    bVar9 = CDisplayML::FormatEmptyLine(*(CDisplayML **)(this + 8),pCVar17);
    if (((bVar9) && ((*(byte *)(lVar4 + 0x134) & 0x40) != 0)) &&
       ((*(uint *)(lVar4 + 200) >> 0x1b & 1) != 0)) {
      lVar12 = CLine::GetHeight(pCVar17,false,(CDisplay *)0x0);
      uVar18 = CDisplay::DVtoIV(*(CDisplay **)(this + 8),lVar12);
    }
  }
  (*(code *)__guard_dispatch_icall_fptr)(this,0);
  *(long *)(this + 0x74) = lVar12;
  *(uint *)(*(longlong *)(this + 8) + 0x74) =
       (uint)bVar24 << 7 | *(uint *)(*(longlong *)(this + 8) + 0x74) & 0xffffff7f;
  *(uint *)(*(longlong *)(this + 8) + 0x74) = *(uint *)(*(longlong *)(this + 8) + 0x74) & 0xffffffbf
  ;
  lVar12 = CDisplayML::CalcScrollHeight(*(CDisplayML **)(this + 8),lVar12,false);
  if (((bVar24 != 0) && ((uVar18 != *(ulonglong *)(this + 0x68) || (lVar12 != lVar10)))) ||
     (lVar10 < lVar12)) {
    *(uint *)(*(longlong *)(this + 8) + 0x74) = *(uint *)(*(longlong *)(this + 8) + 0x74) | 0x100;
  }
  *(ulonglong *)(this + 0x68) = uVar18;
  lVar12 = CDisplay::IVtoDV(*(CDisplay **)(this + 8),uVar18);
  *(long *)(this + 0x20) = lVar12;
  iVar15 = (*(code *)__guard_dispatch_icall_fptr)(this,aCStack_1c8);
  if (((bVar24 != 0) && (iVar21 = *(int *)(this + 0x60), iVar15 != iVar21)) ||
     (iVar21 = *(int *)(this + 0x60), iVar21 < iVar15)) {
    *(int *)(this + 0x60) = iVar15;
    *(uint *)(*(longlong *)(this + 8) + 0x74) = *(uint *)(*(longlong *)(this + 8) + 0x74) | 0x100;
    if (MSO::g_hMso20 == (HINSTANCE__ *)0x0) {
      MSO::g_hMso20 =
           MSO::GetMsoHandleEx(g_hMSO20Inst,L"mso20win32server.dll",L"mso20win32client.dll");
    }
    if ((MSO::g_hMso20 != (HINSTANCE__ *)0xffffffffffffffff) &&
       (MSO::g_hMso20 != (HINSTANCE__ *)0x0)) {
      cVar6 = DAT_18039ffc8 != '\0';
      if (DAT_18039ffc8 < '\0') {
        cVar6 = Ordinal_9342(&DAT_18039ffc8);
      }
      if (cVar6 == '\0') goto LAB_18003e79c;
    }
    cVar6 = (*(code *)__guard_dispatch_icall_fptr)();
    if (((cVar6 == '\0') &&
        (pCVar20 = CRchTxtPtr::GetPF(param_1), ((byte)pCVar20[0x10] & 0xf) - 2 < 2)) &&
       ((*(int *)(*(longlong *)(this + 8) + 0x78) < iVar21 ||
        (*(int *)(*(longlong *)(this + 8) + 0x78) < *(int *)(this + 0x60))))) {
      uStack_1f0 = 0;
      uStack_1e8 = 0;
      uStack_1e0 = 0;
      uStack_1d8 = 0;
      (*(code *)__guard_dispatch_icall_fptr)(this,&uStack_1f0);
    }
  }
LAB_18003e79c:
  if ((bVar24 != 0) || (bVar9 = StartBackgroundRecalc(this), bVar9)) {
    *(undefined8 *)(this + 0x78) = 0xffffffffffffffff;
    *(ushort *)(*(longlong *)(this + 8) + 0xd0) =
         *(ushort *)(*(longlong *)(this + 8) + 0xd0) & 0xfffd;
  }
  CMeasurer::~CMeasurer(aCStack_1c8);
  return true;
}

