@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /W4 /O2 /DNDEBUG /DBLACKBONE_NO_TRACE /EHsc /MT /I sources\blackbone-reference\src native-diagnostics\blackbone_full_probe.cpp /Foartifacts\blackbone-full\full_probe.obj artifacts\blackbone-full\BlackBone.lib /link /OUT:artifacts\blackbone-full\blackbone_full_probe.exe /LIBPATH:sources\blackbone-reference\src\3rd_party\DIA\lib\amd64 /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO advapi32.lib psapi.lib user32.lib ole32.lib oleaut32.lib shell32.lib shlwapi.lib
exit /b %errorlevel%
