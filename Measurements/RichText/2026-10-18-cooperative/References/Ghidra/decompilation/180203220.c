
/* protected: virtual void __cdecl CDisplayEngineML::StepBackgroundRecalc(void) __ptr64 */

void __thiscall CDisplayEngineML::StepBackgroundRecalc(CDisplayEngineML *this)

{
  long lVar1;
  CRchTxtPtr aCStack_68 [16];
  CTxtPtr aCStack_58 [80];
  
  CRchTxtPtr::CRchTxtPtr
            (aCStack_68,*(long *)(this + 0x70),*(CTxtStory **)(*(longlong *)(this + 8) + 0x48));
  lVar1 = CTxtPtr::GetTextLength(aCStack_58);
  RecalcLines(this,aCStack_68,lVar1 - *(int *)(this + 0x70),lVar1 - *(int *)(this + 0x70),true,false
              ,(CLed *)0x0);
  return;
}

