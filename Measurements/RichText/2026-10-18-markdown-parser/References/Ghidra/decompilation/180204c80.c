
/* public: virtual void __cdecl CDisplayML::RecalcLine(long) __ptr64 */

void __thiscall CDisplayML::RecalcLine(CDisplayML *this,long param_1)

{
  CNotifyMgr *this_00;
  undefined4 uStack_28;
  undefined8 uStack_24;
  undefined8 uStack_1c;
  undefined4 uStack_14;
  longlong lStack_10;
  
  this_00 = CDisplay::GetNotifyMgr((CDisplay *)this);
  if (this_00 != (CNotifyMgr *)0x0) {
    lStack_10 = *(longlong *)(this + 0x48);
    uStack_14 = 0;
    uStack_28 = 2;
    uStack_24 = 0;
    uStack_1c = 0;
    if (lStack_10 != 0) {
      CNotifyMgr::NotifyPostReplaceRange
                (this_00,(ITxNotify *)0x0,param_1,0,0,param_1,param_1,(tagNOTIFY_DATA *)&uStack_28);
    }
  }
  return;
}

