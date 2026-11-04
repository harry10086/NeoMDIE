with open("src/ui/MainWindow.h", "r", encoding="utf-8") as f:
    h = f.read()

if "AddTrayIcon" not in h:
    h = h.replace("void ApplyTheme();", "void ApplyTheme();\n    void AddTrayIcon();\n    void RemoveTrayIcon();\n    bool m_hasTrayIcon = false;")
    with open("src/ui/MainWindow.h", "w", encoding="utf-8") as f:
        f.write(h)
    print("MainWindow.h updated")
