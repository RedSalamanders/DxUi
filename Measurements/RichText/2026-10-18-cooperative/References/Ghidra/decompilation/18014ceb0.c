
/* WARNING: Switch with 1 destination removed at 0x00018014ced1 */
/* public: virtual bool __cdecl CDisplayML::WaitForRecalcView(void)const __ptr64 */

bool __thiscall CDisplayML::WaitForRecalcView(CDisplayML *this)

{
  undefined1 uVar1;
  
  if (*(longlong *)(this + 0x48) == 0) {
    return true;
  }
                    /* WARNING: Could not recover jumptable at 0x00018008dfd0. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  uVar1 = (**(code **)(*(longlong *)this + 0x160))
                    (this,0xffffffff,*(int *)(this + 0xc0) + *(int *)(this + 0x7c));
  return (bool)uVar1;
}

