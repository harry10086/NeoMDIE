// ==NeoScript==
// @name         批量重命名 (Batch Rename)
// @description  为当前目录或当前选中的文件/文件夹批量添加前后缀、查找替换、自定义序号模板重命名
// @hotkey       F7
// @toolbar      true
// ==/NeoScript==

/**
 * NeoMDIE 批量文件重命名扩展脚本 (Launcher)
 * 
 * 架构说明:
 * - 由 NeoMDIE 原生脚本引擎调用，自动继承当前活动标签页目录与选定项环境变量
 * - 定位同目录下的 batch_rename.ps1 并通过 PowerShell 启动原生 WinForms 界面。
 */

var shell = new ActiveXObject("WScript.Shell");
var fso = new ActiveXObject("Scripting.FileSystemObject");

// 1. 获取当前脚本所在目录或主程序 scripts 目录
var psFile = "";

// 优先从环境变量获取由 NeoMDIE 传入的 scripts 目录
var envScriptsDir = shell.Environment("PROCESS")("NEOMDIE_SCRIPTS_DIR");
if (!envScriptsDir) {
    envScriptsDir = shell.ExpandEnvironmentStrings("%NEOMDIE_SCRIPTS_DIR%");
    if (envScriptsDir === "%NEOMDIE_SCRIPTS_DIR%") envScriptsDir = "";
}
if (envScriptsDir && fso.FileExists(envScriptsDir + "\\batch_rename.ps1")) {
    psFile = envScriptsDir + "\\batch_rename.ps1";
}

// 尝试从原始脚本路径环境变量获取
if (!psFile) {
    var origScript = shell.Environment("PROCESS")("NEOMDIE_SCRIPT_FILE");
    if (!origScript) origScript = shell.ExpandEnvironmentStrings("%NEOMDIE_SCRIPT_FILE%");
    if (origScript && origScript !== "%NEOMDIE_SCRIPT_FILE%") {
        try {
            var origDir = fso.GetParentFolderName(origScript);
            if (origDir && fso.FileExists(origDir + "\\batch_rename.ps1")) {
                psFile = origDir + "\\batch_rename.ps1";
            }
        } catch(e) {}
    }
}

// 尝试从当前执行脚本路径获取
if (!psFile) {
    try {
        var scriptDir = fso.GetParentFolderName(WScript.ScriptFullName);
        if (scriptDir && fso.FileExists(scriptDir + "\\batch_rename.ps1")) {
            psFile = scriptDir + "\\batch_rename.ps1";
        }
    } catch(e) {}
}

// 尝试常见相对路径
if (!psFile) {
    var candidates = [
        "scripts\\batch_rename.ps1",
        "bin\\scripts\\batch_rename.ps1",
        "..\\scripts\\batch_rename.ps1"
    ];
    for (var i = 0; i < candidates.length; i++) {
        if (fso.FileExists(candidates[i])) {
            psFile = fso.GetAbsolutePathName(candidates[i]);
            break;
        }
    }
}

if (!psFile || !fso.FileExists(psFile)) {
    WScript.Echo("【错误】未找到 batch_rename.ps1 脚本文件，请确认该文件位于 scripts 目录下！");
    WScript.Quit(1);
}

// 3. 启动 PowerShell 隐藏控制台，直接显示 WinForms 窗口
var psCmd = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -STA -WindowStyle Hidden -File "' + psFile + '"';
shell.Run(psCmd, 0, false);
