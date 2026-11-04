with open('src/ui/SettingsDialog.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = 's_hChkZebraStriping = createChk(4, zh ? L"偶数行以交替轻微底色区分 (Zebra Striping)" : L"Alternate row background color (Zebra striping)",\n                                  y4, 345, true);'
addition = '''\n  y4 += lineH + spacing * 2;

  createLabel(4, zh ? L"按键设置与快捷键分类 (MDIE 原版模式):" : L"Keyboard Shortcuts (MDIE Mode):", y4, true);
  y4 += lineH + Theme::Get().Scale(2, hwnd);
  createLabel(4, zh ? L"• 全体快捷键: 全局响应 (Ctrl+T 新建标签, Ctrl+W 关闭标签, Ctrl+Tab 换标签, Alt+D 地址, Ctrl+F 搜索, Ctrl+1..4 分屏, F5 刷新)"
                    : L"• Global Shortcuts: Active everywhere (Ctrl+T New Tab, Ctrl+W, Ctrl+Tab, Alt+D, Ctrl+F, Ctrl+1..4, F5)", y4, false);
  y4 += lineH + spacing;
  createLabel(4, zh ? L"• 列表快捷键: 仅文件列表聚焦时生效 (Space 快速预览, Backspace 返回上一级, Enter 打开, Home/End 顶底, PgUp/Dn 翻页, F2 重命名)"
                    : L"• List Shortcuts: Active in file list (Space preview, Backspace up, Enter open, Home/End top/bottom, PgUp/Dn, F2)", y4, false);'''

if target in text:
    text = text.replace(target, target + addition, 1)
    with open('src/ui/SettingsDialog.cpp', 'w', encoding='utf-8') as f:
        f.write(text)
    print('SettingsDialog updated successfully.')
else:
    print('Target not found in SettingsDialog.')
