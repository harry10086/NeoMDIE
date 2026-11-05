@echo off
REM NeoMDIE - Restore Windows Explorer Default Associations
echo.
echo ========================================================
echo   正在恢复 Windows 默认资源管理器关联，请稍候...
echo ========================================================
echo.

REM 1. Clean open_NeoMDIE subkeys
%SystemRoot%\System32\reg.exe delete "HKCU\Software\Classes\Directory\shell\open_NeoMDIE" /f >nul 2>&1
%SystemRoot%\System32\reg.exe delete "HKCU\Software\Classes\Folder\shell\open_NeoMDIE" /f >nul 2>&1
%SystemRoot%\System32\reg.exe delete "HKCU\Software\Classes\Drive\shell\open_NeoMDIE" /f >nul 2>&1

REM 2. Clean open hijack subkeys
%SystemRoot%\System32\reg.exe delete "HKCU\Software\Classes\Directory\shell\open" /f >nul 2>&1
%SystemRoot%\System32\reg.exe delete "HKCU\Software\Classes\Folder\shell\open" /f >nul 2>&1
%SystemRoot%\System32\reg.exe delete "HKCU\Software\Classes\Drive\shell\open" /f >nul 2>&1
%SystemRoot%\System32\reg.exe delete "HKCU\Software\Classes\CLSID\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}" /f >nul 2>&1

REM 3. Clear default verb on shell keys to eliminate Application Not Found error
%SystemRoot%\System32\reg.exe delete "HKCU\Software\Classes\Directory\shell" /ve /f >nul 2>&1
%SystemRoot%\System32\reg.exe delete "HKCU\Software\Classes\Folder\shell" /ve /f >nul 2>&1
%SystemRoot%\System32\reg.exe delete "HKCU\Software\Classes\Drive\shell" /ve /f >nul 2>&1

REM 4. Delete empty Folder shell key
%SystemRoot%\System32\reg.exe delete "HKCU\Software\Classes\Folder\shell" /f >nul 2>&1

REM 5. Clean launcher script if present
if exist "%APPDATA%\NeoMDIE\open_folder.vbs" del /f /q "%APPDATA%\NeoMDIE\open_folder.vbs" >nul 2>&1

REM 6. Refresh shell icons and associations
%SystemRoot%\System32\ie4uinit.exe -show >nul 2>&1

echo.
echo ========================================================
echo   恢复完成！系统原生资源管理器已完全恢复默认关联。
echo   导致找不到应用程序的残留项已彻底清除。
echo ========================================================
echo.
pause
