// ==NeoScript==
// @name         新建CSV工作表
// @description  在当前活动目录下快速创建一个新的 CSV 工作表文件 (.csv)
// @hotkey       Ctrl+Alt+E
// @toolbar      true
// ==/NeoScript==

/**
 * NeoMDIE 扩展脚本: 新建 CSV 工作表 / 文档
 */

var shell = new ActiveXObject("WScript.Shell");
var fso = new ActiveXObject("Scripting.FileSystemObject");

// 1. 获取当前活动目录
var activePath = shell.Environment("PROCESS")("NEOMDIE_ACTIVE_PATH");
if (!activePath) {
    activePath = shell.ExpandEnvironmentStrings("%NEOMDIE_ACTIVE_PATH%");
    if (activePath === "%NEOMDIE_ACTIVE_PATH%") activePath = "";
}
if (!activePath || !fso.FolderExists(activePath)) {
    try {
        activePath = shell.CurrentDirectory;
    } catch (e) {
        activePath = "";
    }
}

if (!activePath || !fso.FolderExists(activePath)) {
    WScript.Quit(1);
}

// 2. 计算不重复的文件名称
var baseName = "新建表格";
var ext = ".csv";
var targetFile = fso.BuildPath(activePath, baseName + ext);
var index = 2;

while (fso.FileExists(targetFile)) {
    targetFile = fso.BuildPath(activePath, baseName + " (" + index + ")" + ext);
    index++;
}

// 3. 写入带标准 UTF-8 BOM 的表格，支持 Excel、WPS 直接双击打开不乱码
try {
    var stream = new ActiveXObject("ADODB.Stream");
    stream.Type = 2; // adTypeText
    stream.Charset = "UTF-8";
    stream.Open();
    stream.WriteText("序号,项目名称,创建时间,备注\r\n");
    stream.WriteText("1,新建项目A," + new Date().toLocaleString() + ",NeoMDIE 自动创建\r\n");
    stream.SaveToFile(targetFile, 2); // adSaveCreateOverWrite
    stream.Close();
} catch (e) {
    try {
        var ts = fso.CreateTextFile(targetFile, true, true);
        ts.WriteLine("序号,项目名称,创建时间,备注");
        ts.WriteLine("1,新建项目A," + new Date().toLocaleString() + ",NeoMDIE 自动创建");
        ts.Close();
    } catch (e2) {
        WScript.Echo("【错误】创建文件失败: " + e2.message);
    }
}
