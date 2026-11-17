// ==NeoScript==
// @name         多分栏文件比对 (Compare Panes)
// @description  对比当前多窗格同步浏览下的文件夹差异（文件大小、修改时间、存在性等）
// @hotkey       Ctrl+F7
// @toolbar      true
// ==/NeoScript==

/**
 * NeoMDIE 多分栏文件差异比对扩展脚本 (Launcher)
 * 
 * 架构说明:
 * - 由 NeoMDIE 原生脚本引擎调用，自动继承当前活动标签页各分栏路径环境变量
 * - 定位同目录下的 compare_panes.ps1 并通过 PowerShell 启动原生 GUI 界面。
 */

var shell = new ActiveXObject("WScript.Shell");
var fso = new ActiveXObject("Scripting.FileSystemObject");

var psFile = "";

var envScriptsDir = shell.Environment("PROCESS")("NEOMDIE_SCRIPTS_DIR");
if (!envScriptsDir) {
    envScriptsDir = shell.ExpandEnvironmentStrings("%NEOMDIE_SCRIPTS_DIR%");
    if (envScriptsDir === "%NEOMDIE_SCRIPTS_DIR%") envScriptsDir = "";
}
if (envScriptsDir && fso.FileExists(envScriptsDir + "\\compare_panes.ps1")) {
    psFile = envScriptsDir + "\\compare_panes.ps1";
}

if (!psFile) {
    try {
        var scriptDir = fso.GetParentFolderName(WScript.ScriptFullName);
        if (scriptDir && fso.FileExists(scriptDir + "\\compare_panes.ps1")) {
            psFile = scriptDir + "\\compare_panes.ps1";
        }
    } catch(e) {}
}

if (!psFile) {
    var origScript = shell.Environment("PROCESS")("NEOMDIE_SCRIPT_FILE");
    if (!origScript) origScript = shell.ExpandEnvironmentStrings("%NEOMDIE_SCRIPT_FILE%");
    if (origScript && origScript !== "%NEOMDIE_SCRIPT_FILE%") {
        try {
            var origDir = fso.GetParentFolderName(origScript);
            if (origDir && fso.FileExists(origDir + "\\compare_panes.ps1")) {
                psFile = origDir + "\\compare_panes.ps1";
            }
        } catch(e) {}
    }
}

if (psFile) {
    var cmd = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -STA -File "' + psFile + '"';
    shell.Run(cmd, 0, false);
} else {
    shell.Popup("未找到 compare_panes.ps1 界面脚本！\n请检查 scripts 目录文件完整性。", 0, "多分栏比对错误", 16);
}
