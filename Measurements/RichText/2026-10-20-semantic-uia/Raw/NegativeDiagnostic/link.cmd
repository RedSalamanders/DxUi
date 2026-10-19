@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
"C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\link.exe" @"Z:\src\DxUi-worktrees\rich-text-core-d2\.build\reports\rich-semantic-uia-20261019\NativeDispatch-BeforeFix\link.rsp"
exit /b %errorlevel%
