
/* public: virtual bool __cdecl CDisplayML::WaitForRecalc(long,long)const __ptr64 */

bool __thiscall CDisplayML::WaitForRecalc(CDisplayML *this,long param_1,long param_2)

{
  longlong *plVar1;
  longlong lVar2;
  longlong lVar3;
  CDisplayEnginePTS *this_00;
  bool bVar4;
  undefined1 uVar5;
  longlong lVar6;
  longlong *aplStackX_8 [4];
  
  if (*(longlong *)(this + 0xb0) == 0) {
LAB_18004090b:
    bVar4 = false;
  }
  else {
    if ((*(longlong *)(this + 0x50) == 0) && (*(longlong *)(this + 0x48) != 0)) {
      if (this[0xd2] == (CDisplayML)0x0) {
        lVar2 = *(longlong *)(this + 0x10);
        if (lVar2 != 0) {
          plVar1 = (longlong *)(lVar2 + 0x30);
          lVar6 = lVar2;
          if (plVar1 == (longlong *)0x0) {
            lVar6 = 0;
          }
          if (lVar6 != 0) {
            if (*(short *)(lVar2 + 0x38) == 0) {
              *plVar1 = 0;
              *(undefined8 *)(lVar2 + 0x38) = 0;
              *(undefined8 *)(lVar2 + 0x40) = 0;
            }
            *(short *)(lVar2 + 0x38) = *(short *)(lVar2 + 0x38) + 1;
            for (lVar3 = *plVar1; lVar3 != 0; lVar3 = *(longlong *)(lVar3 + 8)) {
              (*(code *)__guard_dispatch_icall_fptr)(lVar3);
            }
            if (((*(short *)(lVar2 + 0x38) == 1) && (*(longlong *)(lVar6 + 0x150) != 0)) &&
               (*(CMsgCallBack **)(lVar6 + 0x158) != (CMsgCallBack *)0x0)) {
              CMsgCallBack::NotifyEvents(*(CMsgCallBack **)(lVar6 + 0x158),1);
            }
            aplStackX_8[0] = plVar1;
            goto LAB_1800408b7;
          }
        }
        aplStackX_8[0] = (longlong *)0x0;
LAB_1800408b7:
        this[0xd2] = (CDisplayML)0x1;
        this_00 = *(CDisplayEnginePTS **)(this + 0xb0);
        if (*(code **)(*(longlong *)this_00 + 0xa0) == CDisplayEngineML::WaitForRecalc) {
          uVar5 = CDisplayEngineML::WaitForRecalc((CDisplayEngineML *)this_00,param_1,param_2);
        }
        else if (*(code **)(*(longlong *)this_00 + 0xa0) == CDisplayEnginePTS::WaitForRecalc) {
          uVar5 = CDisplayEnginePTS::WaitForRecalc(this_00,param_1,param_2);
        }
        else {
          uVar5 = (*(code *)__guard_dispatch_icall_fptr)();
        }
        this[0xd2] = (CDisplayML)0x0;
        CCallMgr::~CCallMgr((CCallMgr *)aplStackX_8);
        return (bool)uVar5;
      }
      if ((((byte)this[0x74] & 0x80) == 0) ||
         ((*(uint *)(*(longlong *)(this + 0x10) + 0xd0) & 0x100000) == 0)) goto LAB_18004090b;
    }
    bVar4 = true;
  }
  return bVar4;
}

