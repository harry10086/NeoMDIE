@echo off
chcp 65001 >nul
echo 正在恢复 Windows 默认资源管理器文件关联...

reg delete "HKCU\Software\Classes\Directory\shell\open" /f >nul 2>&1
reg delete "HKCU\Software\Classes\Folder\shell\open" /f >nul 2>&1
reg delete "HKCU\Software\Classes\Drive\shell\open" /f >nul 2>&1
reg delete "HKCU\Software\Classes\CLSID\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}" /f >nul 2>&1
reg delete "HKCU\Software\Classes\Directory\shell\open_NeoMDIE" /f >nul 2>&1
reg delete "HKCU\Software\Classes\Folder\shell\open_NeoMDIE" /f >nul 2>&1
reg delete "HKCU\Software\Classes\Drive\shell\open_NeoMDIE" /f >nul 2>&1

echo 刷新 Windows 图标与文件关联缓存...
powershell -Command "[void][System.Reflection.Assembly]::LoadWithPartialName('Microsoft.VisualBasic'); $sh = New-Object -ComObject Shell.Application; $sh.Windows() | ForEach-Object { try { $_.Refresh() } catch {} }" >nul 2>&1

echo ========================================================
echo  已成功恢复！现在双击文件夹即可正常使用 Windows 资源管理器。
echo ========================================================
pause

