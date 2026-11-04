# NeoMDIE (新一代轻量现代化多标签资源管理器)

<p align="center">
  <img src="NeoMDIE.svg" width="128" height="128" alt="NeoMDIE Logo" />
</p>

> **复刻二十年未更新的经典 MDIE（Multi-Document Interface Explorer），面向 Windows 10 与 Windows 11 进行现代化重塑。**
> 
> 原版 MDIE 参考：https://cres.s28.xrea.com/soft/mdie.html

---

## 🌟 核心特性

- ⚡ **极致轻量，极速秒开**：基于现代 C++ 与原生 Win32 / Shell COM 构建，完全避免 Web / Electron / .NET 运行时的膨胀开销，编译产物仅 **~500 KB**，单文件独立免安装绿色运行。
- 🍃 **真正绿色便携，免安装**：解压即用，配置自动存储于程序同级目录（`NeoMDIE.ini`），不修改系统注册表，随 U 盘即插即用。
- 🌐 **中英双语无缝切换 (Bilingual i18n)**：自动识别系统语言（中文/英文），设置中提供中英文实时切换，所有菜单、列名、状态提示与手势提示即时刷新。
- 📌 **收藏夹与链接栏 (Links Bar)**：工具栏下方提供专用链接栏，预设常用位置（桌面、下载、文档、图片、各磁盘驱动器），支持 `[+]` 一键添加当前路径、右键管理与中键新标签打开。
- 🖱️ **原生 OLE 跨窗口拖拽 (Drag & Drop)**：支持在分屏之间、从外部系统 Explorer/桌面拖放文件至 NeoMDIE，或直接拖拽文件到左侧目录树/文件夹上完成移动或复制。
- 🏷️ **F2 就地无缝重命名 (Inline Rename)**：选中文件按 `F2` 直接在列表中编辑文件名，智能预选主文件名并保留扩展名，按 `Enter` 提交、`Esc` 取消。
- ✂️ **系统级剪贴板与原生文件操作**：深度集成 `IFileOperation`，支持 `Ctrl+C`、`Ctrl+X`、`Ctrl+V`、`Delete`（放进回收站）、`Shift+Delete`（永久删除），原生具备撤销与防覆盖确认。
- 📊 **“此电脑”磁盘容量可视化进度条**：详细展示各分区剩余与总容量，并绘制现代 Fluent 风格的存储使用量进度条（超过 90% 自动变红预警）。
- 🌲 **目录树与文件列表双向自动同步 (Tree Auto-Sync)**：启动默认展示左侧树状导航，列表导航到任意子文件夹时，左侧目录树自动展开并高亮对应节点。
- 📑 **现代化多标签页浏览 (Tabs)**：支持平滑拖拽重排、中键点击关闭、双击新建/关闭、历史后退/前进、会话状态记忆与自动恢复。提供右键标签菜单：**固定/锁定标签页**（防误关）、关闭其他标签、关闭右侧标签、复制路径等。
- ⚙️ **多选项卡综合设置面板 (Settings Dialog)**：提供 5 大功能配置分页（[常规]、[文件列表]、[鼠标手势]、[多栏分屏]、[界面外观]），支持深色/浅色主题、默认视图、手势速查等全面设置。
- 🪟 **MDIE 原生级多栏多屏分屏矩阵 (Multi-Pane Grid)**：
  - 支持 **🗖 单栏 (1x1)**、**⫾ 左右双栏 (1x2)**、**⫽ 上下双栏 (2x1)**、**⊞ 田字四分屏 (2x2)**、**⧉ 1左2右三分屏 (1+2)**！
  - **独立窗格标题栏**：每个分屏均配备顶部路径栏、向上一级 `[↑]`、刷新 `[↻]`、最大化/还原 `[🗖]` 与关闭 `[✕]` 操作按钮。
  - **无级拖动调节**：分屏分割条支持实时鼠标拖拽调整宽度/高度比例，双击分割条快速恢复 50%/50% 对称布局。
  - 拥有**活动窗格高亮边框**指示，点击任意分屏即刻切换焦点，标签栏与路径栏实时响应。
- 🖱️ **高灵敏动态 HUD 与轨迹鼠标手势 (Mouse Gestures)**：
  - 右键划动时实时绘制 **发光蓝色轨迹线**，光标侧跟随呈现 **高透发光 HUD 胶囊悬浮窗**，实时反馈当前手势及动作说明。划动操作后智能抑制右键菜单，防止误触。
- 📑 **100% 还原系统原生 Shell 右键上下文菜单**：
  - **文件/文件夹右键**：完美加载 7-Zip、WinRAR、Git、打开方式、发送到等第三方 Shell 扩展及二级自绘子菜单，并在底部集成 NeoMDIE 专有功能（新标签打开、分屏打开、复制路径、打开终端、加到链接栏）。
  - **空白背景右键**：调起系统级空白上下文菜单，拥有完整的**系统原生“新建”文件模板菜单**（Word、Excel、PowerPoint、文本文档、压缩包等）。
- 🔍 **搜索过滤与子文件夹穿透 (Search & Filter)**：按 `Ctrl+F` 快速过滤当前目录内容，提供 `[⚙]` 选项菜单支持 **包含子文件夹（递归深度搜索）** 与 **区分大小写** 匹配。
- 👁️ **Windows 原生文档预览窗格 (Preview Pane)**：按下 `Alt+P` 即可呼出预览窗格，原生对接 Windows `IPreviewHandler` 接口，无缝预览 PDF、Office (Word/Excel/PPT)、图片、音频及各类纯文本代码文件，由系统沙箱进程托管，主程序零崩坏风险。
- 📜 **现代化 JavaScript 扩展脚本引擎**：支持通过编写 `.js` 脚本为 NeoMDIE 扩展功能，提供头部元信息配置（自定义快捷键、注入菜单、工具栏按钮），按 `F12` 可调出脚本调试控制台。
- 🎨 **Win10 / Win11 原生暗黑模式与 Fluent 视觉**：支持跟随系统主题自动切换浅色/深色模式，原生适配沉浸式深色标题栏（DWM Immersive Dark Mode）与高分屏（Per-Monitor V2 DPI Awareness）。
- 💻 **现代命令行工具一键唤起**：可在当前目录一键打开 Windows Terminal、PowerShell、CMD 或 VS Code。

---

## 🖱️ 鼠标手势指南

在文件列表任意区域**按住鼠标右键**并划动即可触发：

| 手势轨迹 | 对应动作 | 说明 |
| :---: | :--- | :--- |
| `←` (向左) | **后退** | 返回上一个浏览的历史路径 |
| `→` (向右) | **前进** | 前进到下一个浏览的历史路径 |
| `↑` (向上) | **向上一级** | 进入父级文件夹 |
| `↓` (向下) | **关闭标签页** | 关闭当前处于焦点的标签页 |
| `↑↓` (上下) | **刷新** | 重新载入当前目录内容 |
| `↓→` (下右) | **新建标签页** | 在当前目录下新建标签页 |

> *注：单击右键无明显划动时，正常弹出 Windows 原生上下文菜单。*

---

## ⌨️ 常用快捷键

| 快捷键 | 功能 |
| :--- | :--- |
| `Ctrl + T` | 新建标签页 |
| `Ctrl + W` | 关闭当前标签页 |
| `Ctrl + K` | 复制当前标签页 |
| `Ctrl + E` | 呼出分屏布局选择菜单 |
| `Ctrl + 1` | 切换为单栏视图 (1x1) |
| `Ctrl + 2` | 切换为左右双栏分屏 (1x2) |
| `Ctrl + 3` | 切换为上下双栏分屏 (2x1) |
| `Ctrl + 4` | 切换为田字四分屏 (2x2) |
| `Ctrl + 5` | 切换为 1左2右三分屏 (1+2) |
| `Alt + P` | 开启 / 隐藏文件预览窗格 |
| `Ctrl + F` | 聚焦即时过滤搜索栏 |
| `Ctrl + L` / `Alt + D` | 快速聚焦地址栏 / 面包屑路径编辑 |
| `F5` / `Ctrl + R` | 刷新当前文件列表 |
| `Alt + Up` / `Backspace` | 返回上一级目录 |
| `Alt + Left` / `Alt + Right` | 历史路径后退 / 前进 |
| `Ctrl + Shift + N` | 在当前目录下新建文件夹 |
| `Ctrl + ~` | 在当前目录打开 Windows Terminal |
| `Ctrl + Shift + R` | 重新载入所有扩展脚本 |
| `F12` | 打开扩展脚本调试控制台 |

---

## 🧩 扩展脚本编写说明

脚本存放于程序同级目录下的 `scripts/` 文件夹中，支持标准 JavaScript 语法与头部元信息：

```javascript
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
        output += fso.GetFileName(files[i]) + "\r\n";
    }
    // 复制到系统剪贴板
    var clipCmd = "powershell.exe -command \"Set-Clipboard -Value @'\n" + output.replace(/'/g, "''") + "'@\"";
    shell.Run(clipCmd, 0, true);
    WScript.Echo("已成功复制到剪贴板！");
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
