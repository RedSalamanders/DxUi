#pragma once
void RunRichTextInputTests();
// Reuses the production-store fixtures in the native input translation unit;
// these rootless cases never request desktop focus.
void RunRichTextStoreTests();
