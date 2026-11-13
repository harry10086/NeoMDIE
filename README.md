# NeoMDIE (新一代轻量现代化多标签资源管理器)

<p align="center">
  <img src="NeoMDIE.svg" width="128" height="128" alt="NeoMDIE Logo" />
</p>

> **复活二十年未更新的经典 MDIE（Multi-Document Interface Explorer），面向 Windows 10 与 Windows 11 进行现代化重塑。**
> 
> 原版 MDIE 参考：https://cres.s28.xrea.com/soft/mdie.html
![2pane](https://github.mianao.info/https://raw.githubusercontent.com/harry10086/picx-images-hosting/master/NeoMDIE/2pane.webp)
---

## 🌟 核心特性

- ⚡ **极致轻量，极速秒开**：基于现代 C++ 与原生 Win32 / Shell COM 构建，完全避免 Web / Electron / .NET 运行时的膨胀开销，编译产物 **~1 MB**，单文件独立免安装绿色运行，内存占用不大于 **~10 MB**。
- 🍃 **真正绿色便携，免安装**：解压即用，配置自动存储于程序同级目录（`NeoMDIE.ini`），随 U 盘即插即用。
- 📌 **链接栏 (Links Bar)**：工具栏下方提供专用链接栏，预设常用位置（桌面、下载、文档、图片、各磁盘驱动器），支持 `[+]` 一键添加当前路径、右键管理与中键新标签打开。
- 🖱️ **原生 OLE 跨窗口拖拽 (Drag & Drop)**：支持在分屏之间、从外部系统 Explorer/桌面拖放文件至 NeoMDIE，或直接拖拽文件到左侧目录树/文件夹上完成移动或复制。
  - 默认拖拽规则（直接鼠标左键拖动）：
    - 同分区/同磁盘（如 C: 到 C:）：默认执行 移动 (MOVE)；
    - 跨分区/跨磁盘（如 C: 到 D:）：默认执行 复制 (COPY)。
  - 键盘修饰键控制：
    - 按住 Shift 拖放：强制 移动 (MOVE)（无论是否跨盘）；
    - 按住 Ctrl 拖放：强制 复制 (COPY)（光标显示带有 + 加号）；
    - 按住 Alt 拖放：在目标位置 创建快捷方式 (LINK)（光标显示小箭头）。
- 🌲 **目录树与文件列表双向自动同步 (Tree Auto-Sync)**：启动默认展示左侧树状导航，列表导航到任意子文件夹时，左侧目录树自动展开并高亮对应节点。
- 📑 **现代化多标签页浏览 (Tabs)**：支持平滑拖拽重排、中键点击关闭、双击新建/关闭、历史后退/前进、会话状态记忆与自动恢复。提供右键标签菜单：**固定/锁定标签页**（防误关）、关闭其他标签、关闭右侧标签、复制路径等。支持 **标签页<->分栏** 拖拽合并和拆分。
- ⚙️ **多选项卡综合设置面板 (Settings Dialog)**：提供 5 大功能配置分页（[常规]、[文件列表]、[鼠标手势]、[多栏分屏]、[界面外观]），支持深色/浅色主题、默认视图、手势速查等全面设置。
- 🪟 **MDIE 原生级多栏多屏分屏矩阵 (Multi-Pane Grid)**：
  - 支持 **🗖 单栏 (1x1)**、**⫾ 左右双栏 (1x2)**、**⫽ 上下双栏 (2x1)**、**⊞ 田字四分屏 (2x2)**、**⧉ 1左2右三分屏 (1+2)**！
  - **独立窗格标题栏**：每个分屏均配备顶部路径栏、最大化/还原 `[🗖]` 与关闭 `[✕]` 操作按钮。
  - **无级拖动调节**：分屏分割条支持实时鼠标拖拽调整宽度/高度比例，双击分割条快速恢复 50%/50% 对称布局。
  - 拥有**活动窗格高亮边框**指示，点击任意分屏即刻切换焦点，标签栏与路径栏实时响应。
- 🖱️ **高灵敏动态 HUD 与轨迹鼠标手势 (Mouse Gestures)**：
  - 右键划动时实时绘制 **发光蓝色轨迹线**，光标侧跟随呈现 **高透发光 HUD 胶囊悬浮窗**，实时反馈当前手势及动作说明。划动操作后智能抑制右键菜单，防止误触。
- 🔍 **搜索过滤与子文件夹穿透 (Search & Filter)**：按 `Ctrl+F` 快速过滤当前目录内容，提供 `[⚙]` 选项菜单支持 **包含子文件夹（递归深度搜索）** 与 **区分大小写** 匹配。
- 👁️ **Windows 原生文档预览窗格 (Preview Pane)**：按下 `Alt+P` 即可呼出预览窗格，原生对接 Windows `IPreviewHandler` 接口，无缝预览 PDF、Office (Word/Excel/PPT)、图片、音频及各类纯文本代码文件，由系统沙箱进程托管，主程序零崩坏风险。
- 📜 **现代化 JavaScript 扩展脚本引擎**：支持通过编写 `.js` 脚本为 NeoMDIE 扩展功能，提供头部元信息配置（自定义快捷键、注入菜单、工具栏按钮），按 `F12` 可调出脚本调试控制台。
- 🎨 **Win10 / Win11 原生暗黑模式与 Fluent 视觉**：支持跟随系统主题自动切换浅色/深色模式，原生适配沉浸式深色标题栏（DWM Immersive Dark Mode）与高分屏（Per-Monitor V2 DPI Awareness）。

---

## 🖱️ 鼠标手势指南

在文件列表任意区域**按住鼠标右键**并划动即可触发：

| 手势轨迹 | 对应动作 | 说明 |
| :---: | :--- | :--- |
| `←` (向左) | **后退** | 返回上一个浏览的历史路径 |
| `→` (向右) | **前进** | 前进到下一个浏览的历史路径 |
| `↑` (向上) | **向上一级** | 进入父级文件夹 |
| `↓` (向下) | **刷新** | 刷新当前标签页 |
| `→←` (右左) | **新建标签页** | 当前目录新建标签页 |
| `↓→` (下右) | **关闭当前标签页** | 关闭当前标签页 |
| `↑←` (上左) | **显示/隐藏侧边栏目录树** | 显示/隐藏侧边栏目录树 |
| `↑→` (上右) | **显示/隐藏快速预览窗格** | 显示/隐藏快速预览窗格 |
| `↓↑` (下上) | **滚动到列表顶部** | 在当前目录滚动到列表顶部 |
| `↑↓` (上下) | **滚动到列表底部** | 在当前目录滚动到列表顶部 |
| `←↑` (左上) | **向上翻页** | 在当前目录向上翻页 |
| `←↓` (左上) | **向下翻页** | 在当前目录向下翻页 |

> *注：单击右键无明显划动时，正常弹出 Windows 原生上下文菜单。*

---

## ⌨️ 常用快捷键

| 快捷键 | 功能 |
| :--- | :--- |
| `Ctrl + T` | 新建标签页 |
| `Ctrl + W` | 关闭当前标签页 |
| `Ctrl + Tab` | 切换到下一个标签页 |
| `Ctrl + Shift + Tab` | 切换到上一个标签页 |
| `Ctrl + Shift + 1` | 切换为单栏视图 (1x1) |
| `Ctrl + Shift + 2` | 切换为左右双栏分屏 (1x2) |
| `Ctrl + Shift + 3` | 切换为上下双栏分屏 (2x1) |
| `Ctrl + Shift + 4` | 切换为田字四分屏 (2x2) |
| `Alt + D` | 聚焦到地址栏 |
| `Ctrl + F` | 聚焦搜索过滤搜索栏 |
| `F5` | 刷新当前文件列表 |
| `Alt + Enter` | 属性 |
| `Alt + Up` / `Backspace` | 返回上一级目录 |
| `Alt + Left` / `Alt + Right` | 历史路径后退 / 前进 |
| `Ctrl + Shift + N` | 在当前目录下新建文件夹 |
| `Ctrl + Shift + Alt + M` | 在当前目录下新建 markdown 文件 |
| `F11` | 最大化/还原当前栏 |

---

## 🧩 扩展脚本编写说明

脚本存放于程序同级目录下的 `scripts/` 文件夹中，支持标准 JavaScript 语法与头部元信息：

```javascript
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

```

---

## 🛠️ 构建指南

### 环境要求
- Windows 10 或 Windows 11
- Visual Studio 2019 / 2022 Build Tools (包含 MSVC x64 C++ 编译器与 Windows 10 SDK)

### 一键编译
直接运行根目录下的 `build.bat` 脚本：

```cmd
build.bat
```

编译完成后，独立的免安装绿色可执行文件将输出至 `bin\NeoMDIE.exe`。

---

## 📄 开源许可证

本项目基于 [GPL-3.0 许可证](LICENSE) 分发。
