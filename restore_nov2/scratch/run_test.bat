@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /EHsc /MD scratch\test_bg2.cpp /Fe:scratch\test_bg2.exe shell32.lib ole32.lib user32.lib
if exist scratch\test_bg2.exe (
    scratch\test_bg2.exe
)
