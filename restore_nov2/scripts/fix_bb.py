# Restore BreadcrumbBar.cpp correctly with CS_HREDRAW
with open("src/ui/BreadcrumbBar.cpp", "r", encoding="utf-8") as f:
    bb = f.read()

if "CS_HREDRAW" not in bb:
    bb = bb.replace("wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);\n        RegisterClassExW(&wc);",
                    "wc.style = CS_HREDRAW | CS_VREDRAW;\n        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);\n        RegisterClassExW(&wc);")
    with open("src/ui/BreadcrumbBar.cpp", "w", encoding="utf-8") as f:
        f.write(bb)
print("BreadcrumbBar.cpp checked")
