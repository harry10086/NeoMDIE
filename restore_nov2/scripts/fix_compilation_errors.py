# 1. Fix Config.cpp
with open("src/core/Config.cpp", "r", encoding="utf-8") as f:
    cc = f.read()

cc = cc.replace("actionTreeOpen = GetPrivateProfileIntW(secGeneral, L\"ActionTreeOpen\", 0, m_iniPath.c_str());",
                "actionTree.click = GetPrivateProfileIntW(secGeneral, L\"ActionTree_Click\", 0, m_iniPath.c_str());")
cc = cc.replace("actionLinksOpen = GetPrivateProfileIntW(secGeneral, L\"ActionLinksOpen\", 0, m_iniPath.c_str());",
                "actionLinks.click = GetPrivateProfileIntW(secGeneral, L\"ActionLinks_Click\", 0, m_iniPath.c_str());")
cc = cc.replace("actionAddressOpen = GetPrivateProfileIntW(secGeneral, L\"ActionAddressOpen\", 0, m_iniPath.c_str());",
                "actionAddress.click = GetPrivateProfileIntW(secGeneral, L\"ActionAddress_Click\", 0, m_iniPath.c_str());")

cc = cc.replace("writeInt(secGeneral, L\"ActionTreeOpen\", actionTreeOpen);",
                "writeInt(secGeneral, L\"ActionTree_Click\", actionTree.click);")
cc = cc.replace("writeInt(secGeneral, L\"ActionLinksOpen\", actionLinksOpen);",
                "writeInt(secGeneral, L\"ActionLinks_Click\", actionLinks.click);")
cc = cc.replace("writeInt(secGeneral, L\"ActionAddressOpen\", actionAddressOpen);",
                "writeInt(secGeneral, L\"ActionAddress_Click\", actionAddress.click);")

with open("src/core/Config.cpp", "w", encoding="utf-8") as f:
    f.write(cc)
print("Config.cpp fixed")

# 2. Fix DirectoryTree.cpp
with open("src/ui/DirectoryTree.cpp", "r", encoding="utf-8") as f:
    dt = f.read()

if '#include "..\\core\\Config.h"' not in dt:
    dt = dt.replace('#include "Theme.h"', '#include "Theme.h"\n#include "..\\core\\Config.h"')
    with open("src/ui/DirectoryTree.cpp", "w", encoding="utf-8") as f:
        f.write(dt)
print("DirectoryTree.cpp fixed")

# 3. Fix SettingsDialog.cpp (BrushAccent -> BrushSelected)
with open("src/ui/SettingsDialog.cpp", "r", encoding="utf-8") as f:
    sd = f.read()

sd = sd.replace("Theme::Get().BrushAccent()", "Theme::Get().BrushSelected()")
with open("src/ui/SettingsDialog.cpp", "w", encoding="utf-8") as f:
    f.write(sd)
print("SettingsDialog.cpp fixed")

# 4. Fix MainWindow.cpp
with open("src/ui/MainWindow.cpp", "r", encoding="utf-8") as f:
    mw = f.read()

mw = mw.replace("Config::Get().actionAddressOpen", "Config::Get().actionAddress.click")
mw = mw.replace("Config::Get().actionTreeOpen", "Config::Get().actionTree.click")

mw = mw.replace("m_tabBar.SetTabPinned(actIdx, !m_tabBar.IsTabPinned(actIdx));",
                "m_tabBar.TogglePinTab(actIdx);")
mw = mw.replace("m_tabBar.SelectNextTab();", "m_tabBar.NextTab();")
mw = mw.replace("m_tabBar.SelectPrevTab();", "m_tabBar.PrevTab();")

with open("src/ui/MainWindow.cpp", "w", encoding="utf-8") as f:
    f.write(mw)
print("MainWindow.cpp fixed")
