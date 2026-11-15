@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /W4 /EHsc /MT /I sources\blackbone-reference\src native-diagnostics\blackbone_native_probe.cpp /Foartifacts\blackbone-user-mode\probe.obj artifacts\blackbone-user-mode\obj\NativeSubsystem.obj artifacts\blackbone-user-mode\obj\Utils.obj /link /OUT:artifacts\blackbone-user-mode\blackbone_native_probe.exe /OPT:REF /OPT:ICF /LTCG /INCREMENTAL:NO advapi32.lib psapi.lib user32.lib
exit /b %errorlevel%
