#pragma once
#include "..\Common.h"

enum class PreferredAppMode {
    Default = 0,
    AllowDark = 1,
    ForceDark = 2,
    ForceLight = 3,
    Max = 4
};

class Theme {
public:
    static Theme& Get();

    void Initialize();
    bool IsDarkMode() const { return m_isDark; }
    void SetDarkMode(bool dark);
    void ToggleTheme();

    void ApplyToWindow(HWND hwnd);

    // DPI Scaling
    int Scale(int val, HWND hwnd = nullptr) const;
    UINT GetDpi(HWND hwnd = nullptr) const;
    float GetScaleFactor(HWND hwnd = nullptr) const { return (float)GetDpi(hwnd) / 96.0f; }
    int Unscale(int val, HWND hwnd = nullptr) const { float s = GetScaleFactor(hwnd); return (s > 0.0f) ? (int)((float)val / s + 0.5f) : val; }

    // Colors
    COLORREF ClrBackground() const;
    COLORREF ClrSurface() const;
    COLORREF ClrHover() const;
    COLORREF ClrSelected() const;
    COLORREF ClrBorder() const;
    COLORREF ClrTextPrimary() const;
    COLORREF ClrTextSecondary() const;
    COLORREF ClrAccent() const;

    // Brushes & Pens
    HBRUSH BrushBackground() const { return m_hbrBg; }
    HBRUSH BrushSurface() const { return m_hbrSurface; }
    HBRUSH BrushHover() const { return m_hbrHover; }
    HBRUSH BrushSelected() const { return m_hbrSelected; }
    HBRUSH BrushBorder() const { return m_hbrBorder; }
    HBRUSH BrushAccent() const { return m_hbrAccent; }
    HPEN   PenBorder() const { return m_hpenBorder; }

    void RecreateGdiObjects();

    HFONT GetUIFont(HWND hwnd = nullptr);
    HFONT GetUIFontBold(HWND hwnd = nullptr);
    HFONT GetIconFont(int pixelHeight, HWND hwnd = nullptr);

private:
    Theme();
    ~Theme();

    void DetectSystemDarkMode();

    bool m_isDark = false;
    bool m_autoMode = true;

    HBRUSH m_hbrBg = nullptr;
    HBRUSH m_hbrSurface = nullptr;
    HBRUSH m_hbrHover = nullptr;
    HBRUSH m_hbrSelected = nullptr;
    HBRUSH m_hbrBorder = nullptr;
    HBRUSH m_hbrAccent = nullptr;
    HPEN   m_hpenBorder = nullptr;

    HFONT  m_fontUI = nullptr;
    HFONT  m_fontUIBold = nullptr;
    int    m_lastFontDpi = 96;

    // UxTheme function pointers
    using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode);
    using fnAllowDarkModeForWindow = bool(WINAPI*)(HWND, bool);
    using fnShouldAppsUseDarkMode = bool(WINAPI*)();
    using fnRefreshImmersiveColorPolicyState = void(WINAPI*)();

    fnSetPreferredAppMode m_pfnSetPreferredAppMode = nullptr;
    fnAllowDarkModeForWindow m_pfnAllowDarkModeForWindow = nullptr;
    fnShouldAppsUseDarkMode m_pfnShouldAppsUseDarkMode = nullptr;
    fnRefreshImmersiveColorPolicyState m_pfnRefreshPolicy = nullptr;
};
