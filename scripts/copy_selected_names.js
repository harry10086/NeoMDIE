// ==NeoScript==
// @name         复制选中文件名
// @description  将所有选中的文件或文件夹名称提取并复制到系统剪贴板
// @hotkey       Ctrl+Shift+C
// @toolbar      true
// ==/NeoScript==

/**
 * NeoMDIE 扩展脚本: 复制选中文件名
 */

var shell = new ActiveXObject("WScript.Shell");
var fso = new ActiveXObject("Scripting.FileSystemObject");

var activePath = shell.Environment("PROCESS")("NEOMDIE_ACTIVE_PATH");
var selectedFiles = shell.Environment("PROCESS")("NEOMDIE_SELECTED_FILES");

if (!activePath) {
    activePath = shell.ExpandEnvironmentStrings("%NEOMDIE_ACTIVE_PATH%");
    if (activePath === "%NEOMDIE_ACTIVE_PATH%") activePath = "";
}
if (!selectedFiles) {
    selectedFiles = shell.ExpandEnvironmentStrings("%NEOMDIE_SELECTED_FILES%");
    if (selectedFiles === "%NEOMDIE_SELECTED_FILES%") selectedFiles = "";
}

if (selectedFiles && selectedFiles.length > 0) {
    var files = selectedFiles.split("|");
    var names = [];
    for (var i = 0; i < files.length; i++) {
        var f = files[i];
        if (f && f.length > 0) {
            var name = fso.GetFileName(f);
            if (!name) {
                var clean = f.replace(/[\\/]+$/, "");
                var lastSlash = clean.lastIndexOf("\\");
                if (lastSlash === -1) lastSlash = clean.lastIndexOf("/");
                name = (lastSlash !== -1) ? clean.substring(lastSlash + 1) : clean;
            }
            if (name) {
                names.push(name);
            }
        }
    }

    if (names.length > 0) {
        var output = names.join("\r\n");

        var copied = false;
        // 方法 1: 使用 Windows 原生 clip.exe (极速，无窗口弹出)
        try {
            var exec = shell.Exec("%windir%\\system32\\clip.exe");
            exec.StdIn.Write(output);
            exec.StdIn.Close();
            while (exec.Status === 0) {
                WScript.Sleep(20);
            }
            copied = true;
        } catch (e1) {
            copied = false;
        }

        // 方法 2: 备用方案，通过 PowerShell Set-Clipboard
        if (!copied) {
            try {
                var tempFile = fso.GetSpecialFolder(2) + "\\neomdie_clip_" + new Date().getTime() + ".txt";
                var ts = fso.CreateTextFile(tempFile, true, true); // Unicode UTF-16
                ts.Write(output);
                ts.Close();
                var cmd = 'powershell.exe -NoProfile -STA -Command "Set-Clipboard -Value (Get-Content -LiteralPath \'' + tempFile + '\' -Raw -Encoding Unicode); Remove-Item -LiteralPath \'' + tempFile + '\' -Force"';
                shell.Run(cmd, 0, true);
                copied = true;
            } catch (e2) {}
        }
    }
}
