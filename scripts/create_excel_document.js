// ==NeoScript==
// @name         新建Excel工作表
// @description  在当前活动的文件夹目录中创建一个新的 Excel 表格文档并自动打开
// @hotkey       Ctrl+Shift+E
// @toolbar      true
// ==/NeoScript==

/**
 * ============================================================================
 * NeoMDIE 扩展脚本模板: 新建 Excel 工作表文档
 * ============================================================================
 * 
 * 脚本说明:
 * 本脚本演示了如何使用 NeoMDIE 的内置脚本引擎与 Windows WSH / COM 技术交互。
 * 
 * 环境变量说明:
 * - NEOMDIE_ACTIVE_PATH   : 当前标签页或活动分栏所浏览的文件系统绝对路径
 * - NEOMDIE_SELECTED_FILES: 当前选中的文件列表 (以 "|" 竖线分隔的完整路径)
 * 
 * 依赖组件:
 * - WScript.Shell (系统内置)
 * - Scripting.FileSystemObject (系统内置)
 * - Excel.Application (若已安装 Microsoft Office / WPS Office 则调用，未安装则创建标准 CSV/XML 表格)
 */

var shell = new ActiveXObject("WScript.Shell");
var fso = new ActiveXObject("Scripting.FileSystemObject");

// 1. 获取当前 NeoMDIE 活动窗口的路径
var activePath = shell.Environment("PROCESS")("NEOMDIE_ACTIVE_PATH");

if (!activePath || !fso.FolderExists(activePath)) {
    WScript.Echo("【错误】无法获取当前有效的文件夹路径: " + activePath);
} else {
    // 2. 计算不重复的 Excel 文件名称 (例如: 新建表格.xlsx, 新建表格 (2).xlsx)
    var baseName = "新建表格";
    var ext = ".xlsx";
    var targetFile = fso.BuildPath(activePath, baseName + ext);
    var index = 2;

    while (fso.FileExists(targetFile)) {
        targetFile = fso.BuildPath(activePath, baseName + " (" + index + ")" + ext);
        index++;
    }

    var created = false;

    // 3. 尝试使用 Excel / Office COM 自动化组件创建标准的 .xlsx 工作簿
    try {
        var excelApp = new ActiveXObject("Excel.Application");
        if (excelApp) {
            excelApp.Visible = false;
            excelApp.DisplayAlerts = false;
            var workbook = excelApp.Workbooks.Add();
            // 保存为标准工作簿格式 (51 = xlOpenXMLWorkbook, 即 .xlsx)
            workbook.SaveAs(targetFile, 51);
            workbook.Close();
            excelApp.Quit();
            created = true;
        }
    } catch (e) {
        // 如果系统没有安装完整 Office Excel COM 组件，使用轻量 CSV 模式生成
        created = false;
    }

    // 4. 若无 Excel COM，降级创建兼容所有办公软件的标准 CSV 表格
    if (!created) {
        var csvFile = targetFile.replace(/\.xlsx$/i, ".csv");
        try {
            var ts = fso.CreateTextFile(csvFile, true, true); // Unicode 文本流
            ts.WriteLine("序号,名称,创建时间,备注");
            ts.WriteLine("1,项目A," + new Date().toLocaleString() + ",NeoMDIE 自动生成");
            ts.Close();
            targetFile = csvFile;
            created = true;
        } catch (err) {
            WScript.Echo("【错误】创建表格文件失败: " + err.message);
        }
    }

    // 5. 提示并在系统关联程序中打开生成的文件
    if (created) {
        WScript.Echo("【成功】已成功在当前目录下创建文件:\n" + targetFile);
        // 调用默认程序打开该 Excel 表格
        shell.Run('"' + targetFile + '"');
    }
}

