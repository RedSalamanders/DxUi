#pragma once
// ASan Debug must instrument every translation unit, including archive and test code.
#ifndef __SANITIZE_ADDRESS__
#error ASan Debug requires compiler AddressSanitizer instrumentation.
#endif
