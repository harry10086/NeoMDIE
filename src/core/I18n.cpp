#include "I18n.h"
#include "Config.h"

I18n& I18n::Get() {
    static I18n instance;
    return instance;
}

I18n::I18n() {
}

void I18n::Initialize() {
    int savedLang = Config::Get().language;
    if (savedLang >= 0 && savedLang <= 3) {
        m_lang = (Language)savedLang;
    } else {
        LANGID uiLang = GetUserDefaultUILanguage();
        LANGID primary = PRIMARYLANGID(uiLang);
        LANGID sub = SUBLANGID(uiLang);
        if (primary == LANG_JAPANESE) {
            m_lang = Language::Japanese;
        } else if (primary == LANG_CHINESE) {
            if (sub == SUBLANG_CHINESE_TRADITIONAL ||
                sub == SUBLANG_CHINESE_HONGKONG ||
                sub == SUBLANG_CHINESE_MACAU) {
                m_lang = Language::TraditionalChinese;
            } else {
                m_lang = Language::Chinese;
            }
        } else {
            m_lang = Language::English;
        }
    }
}

void I18n::SetLanguage(Language lang) {
    if (m_lang != lang) {
        m_lang = lang;
        Config::Get().language = (int)lang;
        Config::Get().Save();
        if (onLanguageChanged) {
            onLanguageChanged();
        }
    }
}

const wchar_t* I18n::Pick(const wchar_t* zhCN, const wchar_t* zhTW, const wchar_t* en, const wchar_t* ja) const {
    switch (m_lang) {
    case Language::Chinese:            return zhCN;
    case Language::TraditionalChinese: return zhTW;
    case Language::Japanese:           return ja;
    case Language::English:
    default:                           return en;
    }
}

const wchar_t* I18n::T(StringId id) const {
    switch (id) {
    case StringId::AppName:            return L"NeoMDIE";
    case StringId::ThisPC:             return Pick(L"此电脑", L"本機", L"This PC", L"PC");
    case StringId::QuickAccess:        return Pick(L"快速访问", L"快速存取", L"Quick Access", L"クイック アクセス");
    case StringId::Desktop:            return Pick(L"桌面", L"桌面", L"Desktop", L"デスクトップ");
    case StringId::Downloads:          return Pick(L"下载", L"下載", L"Downloads", L"ダウンロード");
    case StringId::Documents:          return Pick(L"文档", L"文件", L"Documents", L"ドキュメント");
    case StringId::Pictures:           return Pick(L"图片", L"圖片", L"Pictures", L"ピクチャ");
    case StringId::Music:              return Pick(L"音乐", L"音樂", L"Music", L"ミュージック");
    case StringId::Videos:             return Pick(L"视频", L"影片", L"Videos", L"ビデオ");
    case StringId::NewFolder:          return Pick(L"新建文件夹", L"新增資料夾", L"New Folder", L"新しいフォルダー");
    case StringId::NewTextFile:        return Pick(L"新建 TXT 文档.txt", L"新增文字文件.txt", L"New TXT Document.txt", L"新しいテキスト ドキュメント.txt");
    case StringId::Folder:             return Pick(L"文件夹", L"資料夾", L"Folder", L"フォルダー");
    case StringId::DriveLocal:         return Pick(L"本地磁盘", L"本機磁碟", L"Local Disk", L"ローカル ディスク");
    case StringId::DriveRemovable:     return Pick(L"可移动磁盘", L"卸除式磁碟", L"Removable Disk", L"リムーバブル ディスク");
    case StringId::DriveCD:            return Pick(L"CD/DVD 驱动器", L"CD/DVD 光碟機", L"CD/DVD Drive", L"CD/DVD ドライブ");
    case StringId::DriveNetwork:       return Pick(L"网络驱动器", L"網路磁碟機", L"Network Drive", L"ネットワーク ドライブ");

    case StringId::ColName:            return Pick(L"名称", L"名稱", L"Name", L"名前");
    case StringId::ColDateModified:    return Pick(L"修改日期", L"修改日期", L"Date modified", L"更新日時");
    case StringId::ColType:            return Pick(L"类型", L"類型", L"Type", L"種類");
    case StringId::ColSize:            return Pick(L"大小", L"大小", L"Size", L"サイズ");

    case StringId::BtnTree:            return Pick(L"📁 目录树", L"📁 目錄樹", L"📁 Tree", L"📁 ツリー");
    case StringId::BtnSplit:           return Pick(L"⊞ 分栏", L"⊞ 分割", L"⊞ Split", L"⊞ 分割");
    case StringId::BtnPreview:         return Pick(L"👁 预览", L"👁 預覽", L"👁 Preview", L"👁 プレビュー");

    case StringId::FilterCue:          return Pick(L"即时过滤 (Ctrl+F)", L"即時篩選 (Ctrl+F)", L"Filter files (Ctrl+F)", L"フィルター (Ctrl+F)");
    case StringId::StatusItems:        return Pick(L"%d 个项目", L"%d 個項目", L"%d items", L"%d 個の項目");
    case StringId::StatusSize:         return Pick(L"大小: %s", L"大小: %s", L"Size: %s", L"サイズ: %s");
    case StringId::StatusReady:        return Pick(L"就绪", L"就緒", L"Ready", L"準備完了");

    case StringId::PreviewSelectHint:  return Pick(L"选择文件以预览", L"選擇檔案以預覽", L"Select a file to preview", L"プレビューするファイルを選択");
    case StringId::PreviewNoHandler:   return Pick(L"该文件暂无预览插件", L"此檔案暫無預覽外掛程式", L"No preview available for this file", L"このファイルのプレビューはありません");

    case StringId::GestureBack:        return Pick(L"← 后退", L"← 上一頁", L"← Back", L"← 戻る");
    case StringId::GestureForward:     return Pick(L"→ 前进", L"→ 下一頁", L"→ Forward", L"→ 進む");
    case StringId::GestureUp:          return Pick(L"↑ 向上一级", L"↑ 上一層", L"↑ Up", L"↑ 上へ");
    case StringId::GestureCloseTab:    return Pick(L"↓ 关闭标签", L"↓ 關閉分頁", L"↓ Close Tab", L"↓ タブを閉じる");
    case StringId::GestureRefresh:     return Pick(L"↑↓ 刷新", L"↑↓ 重新整理", L"↑↓ Refresh", L"↑↓ 更新");
    case StringId::GestureNewTab:      return Pick(L"↓→ 新建标签", L"↓→ 新增分頁", L"↓→ New Tab", L"↓→ 新しいタブ");

    case StringId::MenuFile:           return Pick(L"文件(&F)", L"檔案(&F)", L"File(&F)", L"ファイル(&F)");
    case StringId::MenuNewTab:         return Pick(L"新建标签页(&T)\tCtrl+T", L"新增分頁(&T)\tCtrl+T", L"New Tab(&T)\tCtrl+T", L"新しいタブ(&T)\tCtrl+T");
    case StringId::MenuCloseTab:       return Pick(L"关闭当前标签页(&W)\tCtrl+W", L"關閉目前分頁(&W)\tCtrl+W", L"Close Current Tab(&W)\tCtrl+W", L"現在のタブを閉じる(&W)\tCtrl+W");
    case StringId::MenuCloseOtherTabs: return Pick(L"关闭其他标签页(&O)", L"關閉其他分頁(&O)", L"Close Other Tabs(&O)", L"他のタブを閉じる(&O)");
    case StringId::MenuCloseAllTabs:   return Pick(L"关闭所有标签页(&A)", L"關閉所有分頁(&A)", L"Close All Tabs(&A)", L"すべてのタブを閉じる(&A)");
    case StringId::MenuReopenClosedTab:return Pick(L"重新打开已关闭标签页(&Z)\tCtrl+Shift+T", L"重新開啟已關閉分頁(&Z)\tCtrl+Shift+T", L"Reopen Closed Tab(&Z)\tCtrl+Shift+T", L"閉じたタブを再度開く(&Z)\tCtrl+Shift+T");
    case StringId::MenuDuplicateTab:   return Pick(L"复制标签页(&D)\tCtrl+K", L"複製分頁(&D)\tCtrl+K", L"Duplicate Tab(&D)\tCtrl+K", L"タブを複製(&D)\tCtrl+K");
    case StringId::MenuPinTab:         return Pick(L"固定/锁定标签页(&P)", L"釘選分頁(&P)", L"Pin Tab(&P)", L"タブを固定(&P)");
    case StringId::MenuUnpinTab:       return Pick(L"取消固定标签页(&P)", L"取消釘選分頁(&P)", L"Unpin Tab(&P)", L"タブの固定を解除(&P)");
    case StringId::MenuNewFolder:      return Pick(L"新建文件夹(&N)\tCtrl+Shift+N", L"新增資料夾(&N)\tCtrl+Shift+N", L"New Folder(&N)\tCtrl+Shift+N", L"新しいフォルダー(&N)\tCtrl+Shift+N");
    case StringId::MenuNewFile:        return Pick(L"新建文本文件(&M)", L"新增文字文件(&M)", L"New Text Document(&M)", L"新しいテキスト ドキュメント(&M)");
    case StringId::MenuProperties:     return Pick(L"属性(&R)\tAlt+Enter", L"內容(&R)\tAlt+Enter", L"Properties(&R)\tAlt+Enter", L"プロパティ(&R)\tAlt+Enter");
    case StringId::MenuExit:           return Pick(L"退出(&X)\tAlt+F4", L"結束(&X)\tAlt+F4", L"Exit(&X)\tAlt+F4", L"終了(&X)\tAlt+F4");

    case StringId::MenuEdit:           return Pick(L"编辑(&E)", L"編輯(&E)", L"Edit(&E)", L"編集(&E)");
    case StringId::MenuCut:            return Pick(L"剪切(&T)\tCtrl+X", L"剪下(&T)\tCtrl+X", L"Cut(&T)\tCtrl+X", L"切り取り(&T)\tCtrl+X");
    case StringId::MenuCopy:           return Pick(L"复制(&C)\tCtrl+C", L"複製(&C)\tCtrl+C", L"Copy(&C)\tCtrl+C", L"コピー(&C)\tCtrl+C");
    case StringId::MenuCopyName:       return Pick(L"复制选中文件名(&N)\tCtrl+Shift+C", L"複製選取檔案名稱(&N)\tCtrl+Shift+C", L"Copy Selected File Name(&N)\tCtrl+Shift+C", L"選択したファイル名をコピー(&N)\tCtrl+Shift+C");
    case StringId::MenuCopyDir:        return Pick(L"复制所在目录(&D)\tCtrl+Alt+C", L"複製所在目錄(&D)\tCtrl+Alt+C", L"Copy Directory Path(&D)\tCtrl+Alt+C", L"フォルダーのパスをコピー(&D)\tCtrl+Alt+C");
    case StringId::MenuPaste:          return Pick(L"粘贴(&P)\tCtrl+V", L"貼上(&P)\tCtrl+V", L"Paste(&P)\tCtrl+V", L"貼り付け(&P)\tCtrl+V");
    case StringId::MenuPasteShortcut:  return Pick(L"粘贴快捷方式(&S)", L"貼上捷徑(&S)", L"Paste Shortcut(&S)", L"ショートカットの貼り付け(&S)");
    case StringId::MenuDelete:         return Pick(L"删除(&D)\tDel", L"刪除(&D)\tDel", L"Delete(&D)\tDel", L"削除(&D)\tDel");
    case StringId::MenuRename:         return Pick(L"重命名(&M)\tF2", L"重新命名(&M)\tF2", L"Rename(&M)\tF2", L"名前の変更(&M)\tF2");
    case StringId::MenuSelectAll:      return Pick(L"全选(&A)\tCtrl+A", L"全選(&A)\tCtrl+A", L"Select All(&A)\tCtrl+A", L"すべて選択(&A)\tCtrl+A");
    case StringId::MenuInvertSelection:return Pick(L"反向选择(&I)", L"反向選取(&I)", L"Invert Selection(&I)", L"選択の切り替え(&I)");
    case StringId::MenuCopyPath:       return Pick(L"复制完整路径(&Y)\tCtrl+Shift+Alt+C", L"複製完整路徑(&Y)\tCtrl+Shift+Alt+C", L"Copy Full Path(&Y)\tCtrl+Shift+Alt+C", L"フルパスをコピー(&Y)\tCtrl+Shift+Alt+C");

    case StringId::MenuView:           return Pick(L"查看(&V)", L"檢視(&V)", L"View(&V)", L"表示(&V)");
    case StringId::MenuRefresh:        return Pick(L"刷新(&R)\tF5", L"重新整理(&R)\tF5", L"Refresh(&R)\tF5", L"最新の情報に更新(&R)\tF5");
    case StringId::MenuTree:           return Pick(L"目录树(&T)", L"目錄樹(&T)", L"Folder Tree(&T)", L"フォルダー ツリー(&T)");
    case StringId::MenuSplit:          return Pick(L"多栏布局(&S)", L"多欄版面(&S)", L"Multi-Pane Split(&S)", L"マルチペイン分割(&S)");
    case StringId::MenuPreview:        return Pick(L"文件预览窗格(&P)\tAlt+P", L"檔案預覽窗格(&P)\tAlt+P", L"Preview Pane(&P)\tAlt+P", L"プレビュー ウィンドウ(&P)\tAlt+P");
    case StringId::MenuFilter:         return Pick(L"即时过滤查找(&F)\tCtrl+F", L"即時篩選搜尋(&F)\tCtrl+F", L"Incremental Filter(&F)\tCtrl+F", L"フィルター検索(&F)\tCtrl+F");
    case StringId::MenuDetails:        return Pick(L"详细信息(&D)", L"詳細資料(&D)", L"Details(&D)", L"詳細(&D)");
    case StringId::MenuLargeIcons:     return Pick(L"大图标(&G)", L"大圖示(&G)", L"Large Icons(&G)", L"大アイコン(&G)");
    case StringId::MenuSmallIcons:     return Pick(L"小图标(&M)", L"小圖示(&M)", L"Small Icons(&M)", L"小アイコン(&M)");
    case StringId::MenuList:           return Pick(L"列表(&L)", L"清單(&L)", L"List(&L)", L"一覧(&L)");
    case StringId::MenuShowHidden:     return Pick(L"显示隐藏文件(&H)", L"顯示隱藏檔案(&H)", L"Show Hidden Files(&H)", L"隠しファイルを表示(&H)");
    case StringId::MenuCalcFolderSize: return Pick(L"后台计算文件夹大小(&C)", L"背景計算資料夾大小(&C)", L"Calculate Folder Sizes(&C)", L"フォルダー サイズを自動計算(&C)");
    case StringId::MenuSortBy:         return Pick(L"排序方式(&O)", L"排序方式(&O)", L"Sort by(&O)", L"並べ替え(&O)");
    case StringId::MenuSortName:       return Pick(L"名称(&N)", L"名稱(&N)", L"Name(&N)", L"名前(&N)");
    case StringId::MenuSortDate:       return Pick(L"修改日期(&D)", L"修改日期(&D)", L"Date modified(&D)", L"更新日時(&D)");
    case StringId::MenuSortType:       return Pick(L"类型(&T)", L"類型(&T)", L"Type(&T)", L"種類(&T)");
    case StringId::MenuSortSize:       return Pick(L"大小(&S)", L"大小(&S)", L"Size(&S)", L"サイズ(&S)");
    case StringId::MenuSortAsc:        return Pick(L"递增(&A)", L"遞增(&A)", L"Ascending(&A)", L"昇順(&A)");
    case StringId::MenuSortDesc:       return Pick(L"递减(&E)", L"遞減(&E)", L"Descending(&E)", L"降順(&E)");

    case StringId::MenuFavorites:      return Pick(L"收藏(&A)", L"最愛(&A)", L"Favorites(&A)", L"お気に入り(&A)");
    case StringId::MenuAddFavorites:   return Pick(L"添加当前位置到收藏夹(&A)...", L"將目前位置加入最愛(&A)...", L"Add Current Location to Favorites(&A)...", L"現在の場所をお気に入りに追加(&A)...");
    case StringId::MenuManageFavorites:return Pick(L"整理链接栏(&O)...", L"整理連結列(&O)...", L"Organize Links(&O)...", L"リンク バーを整理(&O)...");

    case StringId::MenuGo:             return Pick(L"转到(&G)", L"前往(&G)", L"Go(&G)", L"移動(&G)");
    case StringId::MenuBack:           return Pick(L"后退(&B)\tAlt+Left", L"上一頁(&B)\tAlt+Left", L"Back(&B)\tAlt+Left", L"戻る(&B)\tAlt+Left");
    case StringId::MenuForward:        return Pick(L"前进(&F)\tAlt+Right", L"下一頁(&F)\tAlt+Right", L"Forward(&F)\tAlt+Right", L"進む(&F)\tAlt+Right");
    case StringId::MenuUp:             return Pick(L"向上一级(&U)\tAlt+Up", L"上一層(&U)\tAlt+Up", L"Up One Level(&U)\tAlt+Up", L"上へ(&U)\tAlt+Up");
    case StringId::MenuAddressBar:     return Pick(L"转到地址栏(&L)\tCtrl+L", L"移至網址列(&L)\tCtrl+L", L"Focus Address Bar(&L)\tCtrl+L", L"アドレス バーへ移動(&L)\tCtrl+L");

    case StringId::MenuTabs:           return Pick(L"标签页(&B)", L"分頁(&B)", L"Tabs(&B)", L"タブ(&B)");
    case StringId::MenuNextTab:        return Pick(L"下一个标签页(&N)\tCtrl+Tab", L"下一個分頁(&N)\tCtrl+Tab", L"Next Tab(&N)\tCtrl+Tab", L"次のタブ(&N)\tCtrl+Tab");
    case StringId::MenuPrevTab:        return Pick(L"上一个标签页(&P)\tCtrl+Shift+Tab", L"上一個分頁(&P)\tCtrl+Shift+Tab", L"Previous Tab(&P)\tCtrl+Shift+Tab", L"前のタブ(&P)\tCtrl+Shift+Tab");

    case StringId::MenuTools:          return Pick(L"工具(&T)", L"工具(&T)", L"Tools(&T)", L"ツール(&T)");
    case StringId::MenuTerminal:       return Pick(L"在此处打开 Windows Terminal(&W)\tCtrl+`", L"在此處開啟 Windows Terminal(&W)\tCtrl+`", L"Open Windows Terminal Here(&W)\tCtrl+`", L"ここで Windows Terminal を開く(&W)\tCtrl+`");
    case StringId::MenuPowerShell:     return Pick(L"在此处打开 PowerShell(&P)", L"在此處開啟 PowerShell(&P)", L"Open PowerShell Here(&P)", L"ここで PowerShell を開く(&P)");
    case StringId::MenuCmd:            return Pick(L"打开命令CMD(&C)", L"開啟命令提示字元 CMD(&C)", L"Open CMD(&C)", L"コマンド プロンプトを開く(&C)");
    case StringId::MenuVSCode:         return Pick(L"在此处用 VS Code 打开(&V)", L"在此處使用 VS Code 開啟(&V)", L"Open in VS Code Here(&V)", L"ここで VS Code で開く(&V)");
    case StringId::MenuReloadScripts:  return Pick(L"重新载入扩展脚本(&R)\tCtrl+Shift+R", L"重新載入擴充腳本(&R)\tCtrl+Shift+R", L"Reload Extension Scripts(&R)\tCtrl+Shift+R", L"拡張スクリプトを再読み込み(&R)\tCtrl+Shift+R");
    case StringId::MenuScriptConsole:  return Pick(L"扩展脚本控制台(&O)\tF12", L"擴充腳本主控台(&O)\tF12", L"Script Console(&O)\tF12", L"スクリプト コンソール(&O)\tF12");

    case StringId::MenuLanguage:               return Pick(L"语言 / Language", L"語言 / Language", L"Language", L"言語 / Language");
    case StringId::MenuLangChinese:            return L"简体中文";
    case StringId::MenuLangTraditionalChinese: return L"繁體中文";
    case StringId::MenuLangEnglish:            return L"English";
    case StringId::MenuLangJapanese:           return L"日本語";

    case StringId::MenuHelp:           return Pick(L"帮助(&H)", L"說明(&H)", L"Help(&H)", L"ヘルプ(&H)");
    case StringId::MenuHelpShortcuts:  return Pick(L"键盘快捷键指南(&K)...", L"鍵盤快速鍵指南(&K)...", L"Keyboard Shortcuts Cheatsheet(&K)...", L"キーボード ショートカット一覧(&K)...");
    case StringId::MenuHelpGestures:   return Pick(L"鼠标手势说明(&G)...", L"滑鼠手勢說明(&G)...", L"Mouse Gestures Guide(&G)...", L"マウス ジェスチャー ガイド(&G)...");
    case StringId::MenuHelpMdieHome:   return Pick(L"MDIE 官方主页(&M)...", L"MDIE 官方首頁(&M)...", L"MDIE Official Homepage(&M)...", L"MDIE 公式ホームページ(&M)...");
    case StringId::MenuHelpGitHub:     return Pick(L"GitHub 项目主页(&G)...", L"GitHub 專案首頁(&G)...", L"GitHub Project Page(&G)...", L"GitHub プロジェクト ページ(&G)...");
    case StringId::MenuAbout:          return Pick(L"关于 NeoMDIE(&A)...", L"關於 NeoMDIE(&A)...", L"About NeoMDIE(&A)...", L"NeoMDIE について(&A)...");

    case StringId::ShortcutsCheatsheet:
        return Pick(
            L"【NeoMDIE 常用快捷键速查】\n\n"
            L"• 标签页管理:\n"
            L"  Ctrl + T : 新建标签页\n"
            L"  Ctrl + W : 关闭当前标签页\n"
            L"  Ctrl + Shift + T : 重新打开已关闭标签页\n"
            L"  Ctrl + K : 复制当前标签页\n"
            L"  Ctrl + Tab : 下一个标签页\n"
            L"  Ctrl + Shift + Tab : 上一个标签页\n\n"
            L"• 窗格与多栏分屏:\n"
            L"  Ctrl + 1 : 单栏视图\n"
            L"  Ctrl + 2 : 左右双栏\n"
            L"  Ctrl + 3 : 上下双栏\n"
            L"  Ctrl + 4 : 田字四栏\n"
            L"  Ctrl + 5 : 1左2右三栏\n"
            L"  Alt + P : 切换文件预览窗格\n\n"
            L"• 导航与查找:\n"
            L"  Alt + ← / Backspace : 后退\n"
            L"  Alt + → : 前进\n"
            L"  Alt + ↑ : 向上一级\n"
            L"  Ctrl + L / Alt + D / F6 : 转到地址栏\n"
            L"  Ctrl + F : 搜索与即时过滤\n"
            L"  F5 / Ctrl + R : 刷新目录\n\n"
            L"• 文件编辑与操作:\n"
            L"  Ctrl + C / X / V : 复制 / 剪切 / 粘贴\n"
            L"  Ctrl + Shift + C : 复制完整路径\n"
            L"  Del / Shift + Del : 删除 / 永久删除\n"
            L"  F2 : 重命名\n"
            L"  Ctrl + Shift + N : 新建文件夹\n\n"
            L"• 工具与终端:\n"
            L"  Ctrl + ` : 在当前目录打开 Windows Terminal\n"
            L"  F12 : 打开 JS 扩展脚本控制台\n"
            L"  Ctrl + Shift + R : 重新载入扩展脚本",

            L"【NeoMDIE 常用快速鍵速查】\n\n"
            L"• 分頁管理:\n"
            L"  Ctrl + T : 新增分頁\n"
            L"  Ctrl + W : 關閉目前分頁\n"
            L"  Ctrl + Shift + T : 重新開啟已關閉分頁\n"
            L"  Ctrl + K : 複製目前分頁\n"
            L"  Ctrl + Tab : 下一個分頁\n"
            L"  Ctrl + Shift + Tab : 上一個分頁\n\n"
            L"• 窗格與多欄分割:\n"
            L"  Ctrl + 1 : 單欄檢視\n"
            L"  Ctrl + 2 : 左右雙欄\n"
            L"  Ctrl + 3 : 上下雙欄\n"
            L"  Ctrl + 4 : 田字四欄\n"
            L"  Ctrl + 5 : 1左2右三欄\n"
            L"  Alt + P : 切換檔案預覽窗格\n\n"
            L"• 導覽與搜尋:\n"
            L"  Alt + ← / Backspace : 上一頁\n"
            L"  Alt + → : 下一頁\n"
            L"  Alt + ↑ : 上一層\n"
            L"  Ctrl + L / Alt + D / F6 : 移至網址列\n"
            L"  Ctrl + F : 搜尋與即時篩選\n"
            L"  F5 / Ctrl + R : 重新整理目錄\n\n"
            L"• 檔案編輯與操作:\n"
            L"  Ctrl + C / X / V : 複製 / 剪下 / 貼上\n"
            L"  Ctrl + Shift + C : 複製完整路徑\n"
            L"  Del / Shift + Del : 刪除 / 永久刪除\n"
            L"  F2 : 重新命名\n"
            L"  Ctrl + Shift + N : 新增資料夾\n\n"
            L"• 工具與終端機:\n"
            L"  Ctrl + ` : 在目前目錄開啟 Windows Terminal\n"
            L"  F12 : 開啟 JS 擴充腳本主控台\n"
            L"  Ctrl + Shift + R : 重新載入擴充腳本",

            L"【NeoMDIE Keyboard Shortcuts Cheatsheet】\n\n"
            L"• Tab Management:\n"
            L"  Ctrl + T : New Tab\n"
            L"  Ctrl + W : Close Tab\n"
            L"  Ctrl + Shift + T : Reopen Closed Tab\n"
            L"  Ctrl + K : Duplicate Tab\n"
            L"  Ctrl + Tab : Next Tab\n"
            L"  Ctrl + Shift + Tab : Previous Tab\n\n"
            L"• Pane Layouts:\n"
            L"  Ctrl + 1 : Single Pane\n"
            L"  Ctrl + 2 : Dual Vertical Split\n"
            L"  Ctrl + 3 : Dual Horizontal Split\n"
            L"  Ctrl + 4 : Quad Grid Split\n"
            L"  Ctrl + 5 : Three Panes Split\n"
            L"  Alt + P : Toggle Preview Pane\n\n"
            L"• Navigation & Search:\n"
            L"  Alt + ← / Backspace : Back\n"
            L"  Alt + → : Forward\n"
            L"  Alt + ↑ : Up One Level\n"
            L"  Ctrl + L / Alt + D / F6 : Focus Address Bar\n"
            L"  Ctrl + F : Instant Search & Filter\n"
            L"  F5 / Ctrl + R : Refresh\n\n"
            L"• File Operations:\n"
            L"  Ctrl + C / X / V : Copy / Cut / Paste\n"
            L"  Ctrl + Shift + C : Copy Full Path\n"
            L"  Del / Shift + Del : Delete / Permanent Delete\n"
            L"  F2 : Rename\n"
            L"  Ctrl + Shift + N : New Folder\n\n"
            L"• Tools & Terminals:\n"
            L"  Ctrl + ` : Open Windows Terminal Here\n"
            L"  F12 : Script Console\n"
            L"  Ctrl + Shift + R : Reload Extension Scripts",

            L"【NeoMDIE キーボード ショートカット一覧】\n\n"
            L"• タブ管理:\n"
            L"  Ctrl + T : 新しいタブ\n"
            L"  Ctrl + W : 現在のタブを閉じる\n"
            L"  Ctrl + Shift + T : 閉じたタブを再度開く\n"
            L"  Ctrl + K : タブを複製\n"
            L"  Ctrl + Tab : 次のタブ\n"
            L"  Ctrl + Shift + Tab : 前のタブ\n\n"
            L"• ペイン分割:\n"
            L"  Ctrl + 1 : 単一ペイン\n"
            L"  Ctrl + 2 : 左右2分割\n"
            L"  Ctrl + 3 : 上下2分割\n"
            L"  Ctrl + 4 : 4分割\n"
            L"  Ctrl + 5 : 左1右2 (3分割)\n"
            L"  Alt + P : プレビュー ウィンドウ切替\n\n"
            L"• ナビゲーションと検索:\n"
            L"  Alt + ← / Backspace : 戻る\n"
            L"  Alt + → : 進む\n"
            L"  Alt + ↑ : 上のフォルダーへ\n"
            L"  Ctrl + L / Alt + D / F6 : アドレス バーへ移動\n"
            L"  Ctrl + F : インスタント フィルター検索\n"
            L"  F5 / Ctrl + R : 更新\n\n"
            L"• ファイル操作:\n"
            L"  Ctrl + C / X / V : コピー / 切り取り / 貼り付け\n"
            L"  Ctrl + Shift + C : フルパスをコピー\n"
            L"  Del / Shift + Del : 削除 / 完全削除\n"
            L"  F2 : 名前の変更\n"
            L"  Ctrl + Shift + N : 新しいフォルダー\n\n"
            L"• ツールとターミナル:\n"
            L"  Ctrl + ` : ここで Windows Terminal を開く\n"
            L"  F12 : スクリプト コンソール\n"
            L"  Ctrl + Shift + R : 拡張スクリプトを再読み込み"
        );

    case StringId::GesturesCheatsheet:
        return Pick(
            L"【NeoMDIE 鼠标手势使用指南】\n\n"
            L"在文件列表空白处或任意文件项上，按住鼠标【右键】并在屏幕上划动轨迹：\n\n"
            L"  ← (向左划) : 后退到上一访问文件夹\n"
            L"  → (向右划) : 前进到下一文件夹\n"
            L"  ↑ (向上划) : 返回上一级父目录\n"
            L"  ↓ (向下划) : 关闭当前标签页\n"
            L"  ↑↓ (先上后下) : 刷新当前目录\n"
            L"  ↓→ (先下后右) : 新建标签页\n\n"
            L"提示:\n"
            L"您可以在「工具 -> 选项与设置 -> 鼠标手势」中自由添加任意划动方向并绑定自定义操作！",

            L"【NeoMDIE 滑鼠手勢使用指南】\n\n"
            L"在檔案清單空白處或任意檔案項目上，按住滑鼠【右鍵】並在畫面上劃動軌跡：\n\n"
            L"  ← (向左劃) : 返回上一個瀏覽資料夾\n"
            L"  → (向右劃) : 前進到下一個資料夾\n"
            L"  ↑ (向上劃) : 返回上一層父目錄\n"
            L"  ↓ (向下劃) : 關閉目前分頁\n"
            L"  ↑↓ (先上後下) : 重新整理目前目錄\n"
            L"  ↓→ (先下後右) : 新增分頁\n\n"
            L"提示:\n"
            L"您可以在「工具 -> 選項與設定 -> 滑鼠手勢」中自由新增任意劃動方向並綁定自訂動作！",

            L"【NeoMDIE Mouse Gestures Guide】\n\n"
            L"Hold down the 【Right Mouse Button】 anywhere in the file view and stroke:\n\n"
            L"  ← (Left) : Back\n"
            L"  → (Right) : Forward\n"
            L"  ↑ (Up) : Up One Level\n"
            L"  ↓ (Down) : Close Current Tab\n"
            L"  ↑↓ (Up-Down) : Refresh\n"
            L"  ↓→ (Down-Right) : New Tab\n\n"
            L"Tip:\n"
            L"You can customize and add any gesture in Tools -> Options & Settings -> Gestures!",

            L"【NeoMDIE マウス ジェスチャー ガイド】\n\n"
            L"ファイル一覧上で【右クリックを押したまま】マウスを動かします：\n\n"
            L"  ← (左) : 戻る\n"
            L"  → (右) : 進む\n"
            L"  ↑ (上) : 上のフォルダーへ\n"
            L"  ↓ (下) : 現在のタブを閉じる\n"
            L"  ↑↓ (上下) : 更新\n"
            L"  ↓→ (下右) : 新しいタブ\n\n"
            L"ヒント:\n"
            L"「ツール -> オプションと設定 -> マウス ジェスチャー」から自由にカスタマイズできます！"
        );

    case StringId::TabMenuClose:        return Pick(L"关闭标签页", L"關閉分頁", L"Close Tab", L"タブを閉じる");
    case StringId::TabMenuCloseOthers:  return Pick(L"关闭其他标签页", L"關閉其他分頁", L"Close Other Tabs", L"他のタブを閉じる");
    case StringId::TabMenuCloseRight:   return Pick(L"关闭右侧标签页", L"關閉右側分頁", L"Close Tabs to the Right", L"右側のタブを閉じる");
    case StringId::TabMenuPin:          return Pick(L"📌 固定标签页", L"📌 釘選分頁", L"📌 Pin Tab", L"📌 タブを固定");
    case StringId::TabMenuUnpin:        return Pick(L"取消固定标签页", L"取消釘選分頁", L"Unpin Tab", L"タブの固定を解除");
    case StringId::TabMenuDuplicate:    return Pick(L"复制标签页", L"複製分頁", L"Duplicate Tab", L"タブを複製");
    case StringId::TabMenuCopyPath:     return Pick(L"复制路径", L"複製路徑", L"Copy Path", L"パスをコピー");
    case StringId::TabMenuOpenTerminal: return Pick(L"在终端中打开", L"在終端機中開啟", L"Open in Terminal", L"ターミナルで開く");

    case StringId::SettingsTitle:          return Pick(L"NeoMDIE 选项与设置", L"NeoMDIE 選項與設定", L"NeoMDIE Options & Settings", L"NeoMDIE オプションと設定");
    case StringId::SettingsGeneral:        return Pick(L"常规设置", L"一般設定", L"General", L"全般");
    case StringId::SettingsTheme:          return Pick(L"外观主题", L"外觀佈景主題", L"Appearance Theme", L"外観テーマ");
    case StringId::SettingsLanguage:       return Pick(L"界面语言", L"介面語言", L"Interface Language", L"表示言語");
    case StringId::SettingsShowTree:       return Pick(L"启动时默认显示左侧文件目录树", L"啟動時預設顯示左側目錄樹", L"Show Directory Tree on startup", L"起動時にフォルダー ツリーを表示");
    case StringId::SettingsRestoreTabs:    return Pick(L"启动时恢复上次关闭前的标签页", L"啟動時還原上次關閉前的分頁", L"Restore tabs from last session", L"前回のタブを復元");
    case StringId::SettingsCalcFolderSize: return Pick(L"后台自动递归计算文件夹大小", L"背景自動遞迴計算資料夾大小", L"Calculate folder sizes in background", L"フォルダー サイズをバックグラウンド計算");
    case StringId::SettingsGestures:       return Pick(L"启用右键平滑鼠标手势", L"啟用右鍵平滑滑鼠手勢", L"Enable smooth mouse gestures", L"マウス ジェスチャーを有効にする");
    case StringId::SettingsBtnOK:          return Pick(L"确定", L"確定", L"OK", L"OK");
    case StringId::SettingsBtnCancel:      return Pick(L"取消", L"取消", L"Cancel", L"キャンセル");

    case StringId::Layout1x1:           return Pick(L"🗖 单栏视图 (1x1)", L"🗖 單欄檢視 (1x1)", L"🗖 Single Pane (1x1)", L"🗖 単一ペイン (1x1)");
    case StringId::Layout1x2:           return Pick(L"⫾ 左右双栏 (1x2)", L"⫾ 左右雙欄 (1x2)", L"⫾ Dual Vertical (1x2)", L"⫾ 左右2分割 (1x2)");
    case StringId::Layout2x1:           return Pick(L"⫽ 上下双栏 (2x1)", L"⫽ 上下雙欄 (2x1)", L"⫽ Dual Horizontal (2x1)", L"⫽ 上下2分割 (2x1)");
    case StringId::Layout2x2:           return Pick(L"⊞ 田字四栏 (2x2)", L"⊞ 田字四欄 (2x2)", L"⊞ Quad Grid (2x2)", L"⊞ 4分割 (2x2)");
    case StringId::Layout1p2:           return Pick(L"⧉ 1左2右三栏 (1+2)", L"⧉ 1左2右三欄 (1+2)", L"⧉ Three Panes (1+2)", L"⧉ 左1右2 (1+2)");
    case StringId::LayoutSyncNav:       return Pick(L"同步浏览 (所有分栏联动)", L"同步瀏覽 (所有分欄連動)", L"Synchronized Navigation", L"同期ナビゲーション");

    case StringId::AboutDialogText:
        return Pick(
            L"NeoMDIE\n新一代轻量现代化多标签资源管理器 (复刻经典 MDIE)",
            L"NeoMDIE\n新一代輕量現代化多標籤檔案管理員 (致敬經典 MDIE)",
            L"NeoMDIE\nNext-Generation Lightweight Tabbed File Manager (MDIE Remake)",
            L"NeoMDIE\n次世代軽量モダン タブ型ファイラー (MDIE リメイク)"
        );
    }
    return L"";
}
