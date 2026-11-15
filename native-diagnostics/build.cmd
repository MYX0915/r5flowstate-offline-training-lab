@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist artifacts\native-offset-probe mkdir artifacts\native-offset-probe
cl /nologo /std:c++17 /W4 /WX /EHsc /MT /LD native-diagnostics\offset_probe.cpp /Foartifacts\native-offset-probe\offset_probe.obj /link /OUT:artifacts\native-offset-probe\offset_probe.dll /IMPLIB:artifacts\native-offset-probe\offset_probe.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /W4 /WX /EHsc /MT native-diagnostics\probe_host.cpp /Foartifacts\native-offset-probe\probe_host.obj /link /OUT:artifacts\native-offset-probe\offset_probe_host.exe /INCREMENTAL:NO
exit /b %errorlevel%
