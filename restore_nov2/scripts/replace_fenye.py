import os

files_to_check = [
    r'src\ui\MainWindow.cpp',
    r'src\ui\SettingsDialog.cpp',
    r'src\core\I18n.cpp',
    r'src\core\I18n.h'
]

for f in files_to_check:
    if not os.path.exists(f): continue
    with open(f, 'r', encoding='utf-8') as fp:
        content = fp.read()
    
    # Replace occurrences of 分页
    new_content = content.replace('分页操作', '标签页操作')
    new_content = new_content.replace('下一分页', '下一个标签')
    new_content = new_content.replace('上一分页', '上一个标签')
    new_content = new_content.replace('当前分页', '当前标签')
    new_content = new_content.replace('左侧分页', '左侧标签')
    new_content = new_content.replace('右侧分页', '右侧标签')
    new_content = new_content.replace('全部分页', '全部标签')
    new_content = new_content.replace('复制当前分页', '复制当前标签')
    new_content = new_content.replace('固定当前分页', '固定当前标签')
    new_content = new_content.replace('取消固定分页', '取消固定标签')
    new_content = new_content.replace('分页标签', '标签页')
    new_content = new_content.replace('分页', '标签')

    if new_content != content:
        with open(f, 'w', encoding='utf-8') as fp:
            fp.write(new_content)
        print(f"Updated {f}")
