#pragma once
#include "..\Common.h"

class SettingsDialog {
public:
  static void Show(HWND hwndParent, std::function<void()> onSettingsChanged, int initialCategory = 0);

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static void CreateControls(HWND hwnd);
  static void SwitchTab(HWND hwnd, int tabIndex);
  static void SaveSettings(HWND hwnd);
};
