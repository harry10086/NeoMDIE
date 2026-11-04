@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /std:c++17 tools\test_bgmenu.cpp /Fe:bin\test_bgmenu.exe
bin\test_bgmenu.exe
