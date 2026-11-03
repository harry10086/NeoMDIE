// ==NeoScript==
// @name         复制选中文件名
// @description  将所有选中的文件名提取并复制到剪贴板
// @hotkey       Ctrl+Shift+C
// @toolbar      true
// ==/NeoScript==

var shell = new ActiveXObject("WScript.Shell");
var fso = new ActiveXObject("Scripting.FileSystemObject");

var activePath = shell.Environment("PROCESS")("NEOMDIE_ACTIVE_PATH");
var selectedFiles = shell.Environment("PROCESS")("NEOMDIE_SELECTED_FILES");

WScript.Echo("当前路径: " + activePath);

if (!selectedFiles) {
    WScript.Echo("未选择任何文件。");
} else {
    var files = selectedFiles.split("|");
    WScript.Echo("选中了 " + files.length + " 个文件:");
    var output = "";
    for (var i = 0; i < files.length; i++) {
        var name = fso.GetFileName(files[i]);
        WScript.Echo(" - " + name);
        output += name + "\r\n";
    }

    // 复制到剪贴板
    var clipCmd = "powershell.exe -command \"Set-Clipboard -Value @'\n" + output.replace(/'/g, "''") + "'@\"";
    shell.Run(clipCmd, 0, true);
    WScript.Echo("已成功复制到剪贴板！");
}

