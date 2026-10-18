
/* protected: virtual bool __cdecl CDisplayEngineML::IsBackgroundRecalcDone(void)const __ptr64 */

bool __thiscall CDisplayEngineML::IsBackgroundRecalcDone(CDisplayEngineML *this)

{
  int iVar1;
  
  iVar1 = 0;
  if (*(longlong *)(*(longlong *)(this + 8) + 0x48) != 0) {
    iVar1 = *(int *)(*(longlong *)(*(longlong *)(this + 8) + 0x48) + 0x28);
  }
  return iVar1 - *(int *)(this + 0x70) < 1;
}

