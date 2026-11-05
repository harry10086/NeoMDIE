@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo   Building NeoMDIE (Modern C++ Win32 x64 Release)
echo ===================================================

if not exist "bin" mkdir bin
if not exist "bin\scripts" mkdir bin\scripts
copy /y scripts\*.js bin\scripts\ >nul 2>&1
copy /y scripts\*.ps1 bin\scripts\ >nul 2>&1
if exist "bin\NeoMDIE.exe" (
    del /f /q "bin\NeoMDIE.exe.old" >nul 2>&1
    move /y "bin\NeoMDIE.exe" "bin\NeoMDIE.exe.old" >nul 2>&1
)

where cl.exe >nul 2>&1
if %errorlevel% neq 0 (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    )
)
where cl.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Failed to initialize Visual Studio build environment!
    exit /b 1
)

echo [1/2] Compiling Windows Resources...
rc /nologo /c 65001 /fo src\resources\NeoMDIE.res src\resources\NeoMDIE.rc
if errorlevel 1 (
    echo [ERROR] Resource compilation failed!
    exit /b 1
)

echo [2/2] Compiling C++ Sources...
cl /nologo /std:c++17 /EHsc /O2 /MT /W3 /utf-8 ^
   /I src ^
   src\main.cpp ^
   src\core\Config.cpp ^
   src\core\I18n.cpp ^
   src\core\ShellItem.cpp ^
   src\core\ShellUtils.cpp ^
   src\core\DropTarget.cpp ^
   src\core\FileSystemWatcher.cpp ^
   src\core\FolderSizeCalculator.cpp ^
   src\core\GestureEngine.cpp ^
   src\core\PreviewHandlerHost.cpp ^
   src\core\ScriptEngine.cpp ^
   src\ui\Theme.cpp ^
   src\ui\DirectoryTree.cpp ^
   src\ui\SettingsDialog.cpp ^
   src\ui\TabBar.cpp ^
   src\ui\BreadcrumbBar.cpp ^
   src\ui\FilterBar.cpp ^
   src\ui\LinksBar.cpp ^
   src\ui\FileListView.cpp ^
   src\ui\PreviewPane.cpp ^
   src\ui\MultiPaneContainer.cpp ^
   src\ui\MainWindow.cpp ^
   /Fe:bin\NeoMDIE.exe ^
   /link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup ^
   src\resources\NeoMDIE.res

if errorlevel 1 (
    echo [ERROR] Build failed!
    exit /b 1
)

echo.
echo ===================================================
echo   BUILD SUCCESSFUL!
echo   Executable generated at: bin\NeoMDIE.exe
echo ===================================================

del /q *.obj >nul 2>&1
exit /b 0
