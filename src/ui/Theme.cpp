#include "Theme.h"
#include "..\core\Config.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

static ULONG_PTR s_gdiplusToken = 0;

Theme& Theme::Get() {
    static Theme instance;
    return instance;
}

Theme::Theme() {
    HMODULE hUxTheme = GetModuleHandleW(L"uxtheme.dll");
    if (!hUxTheme) {
        hUxTheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    if (hUxTheme) {
        m_pfnRefreshPolicy = (fnRefreshImmersiveColorPolicyState)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(104));
        m_pfnShouldAppsUseDarkMode = (fnShouldAppsUseDarkMode)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(132));
        m_pfnAllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133));
        m_pfnSetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135));
    }
}

Theme::~Theme() {
    Shutdown();
    if (m_hbrBg) DeleteObject(m_hbrBg);
    if (m_hbrSurface) DeleteObject(m_hbrSurface);
    if (m_hbrHover) DeleteObject(m_hbrHover);
    if (m_hbrSelected) DeleteObject(m_hbrSelected);
    if (m_hbrBorder) DeleteObject(m_hbrBorder);
    if (m_hbrAccent) DeleteObject(m_hbrAccent);
    if (m_hpenBorder) DeleteObject(m_hpenBorder);
    if (m_fontUI) DeleteObject(m_fontUI);
    if (m_fontUIBold) DeleteObject(m_fontUIBold);
}

void Theme::Initialize() {
    if (!s_gdiplusToken) {
        Gdiplus::GdiplusStartupInput gdiInput;
        Gdiplus::GdiplusStartup(&s_gdiplusToken, &gdiInput, nullptr);
    }
    if (m_pfnSetPreferredAppMode) {
        m_pfnSetPreferredAppMode(PreferredAppMode::AllowDark);
    }
    if (m_pfnRefreshPolicy) {
        m_pfnRefreshPolicy();
    }
    DetectSystemDarkMode();
    RecreateGdiObjects();
}

void Theme::Shutdown() {
    if (s_gdiplusToken) {
        Gdiplus::GdiplusShutdown(s_gdiplusToken);
        s_gdiplusToken = 0;
    }
}

void Theme::DetectSystemDarkMode() {
    if (m_pfnShouldAppsUseDarkMode) {
        m_isDark = m_pfnShouldAppsUseDarkMode();
        return;
    }

    // Fallback: Read registry
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 1;
        DWORD size = sizeof(val);
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            m_isDark = (val == 0);
        }
        RegCloseKey(hKey);
    }
}

void Theme::SetDarkMode(bool dark) {
    m_isDark = dark;
    m_autoMode = false;
    if (m_pfnSetPreferredAppMode) {
        m_pfnSetPreferredAppMode(dark ? PreferredAppMode::ForceDark : PreferredAppMode::ForceLight);
    }
    using fnFlushMenuThemes = void(WINAPI*)();
    static auto pfnFlush = (fnFlushMenuThemes)GetProcAddress(GetModuleHandleW(L"uxtheme.dll"), MAKEINTRESOURCEA(136));
    if (pfnFlush) pfnFlush();

    RecreateGdiObjects();
}

void Theme::ToggleTheme() {
    SetDarkMode(!m_isDark);
}

void Theme::ApplyToWindow(HWND hwnd) {
    if (!hwnd) return;

    if (m_pfnAllowDarkModeForWindow) {
        m_pfnAllowDarkModeForWindow(hwnd, m_isDark);
    }

    // Windows 10 20H1+ and Windows 11: DWMWA_USE_IMMERSIVE_DARK_MODE = 20
    BOOL useDark = m_isDark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));
    // Compatibility for Windows 10 1809 - 1909: DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 = 19
    DwmSetWindowAttribute(hwnd, 19, &useDark, sizeof(useDark));

    // Update window theme
    SetWindowTheme(hwnd, m_isDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
}

UINT Theme::GetDpi(HWND hwnd) const {
    if (hwnd) {
        using fnGetDpiForWindow = UINT(WINAPI*)(HWND);
        static auto pfn = (fnGetDpiForWindow)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
        if (pfn) return pfn(hwnd);
    }
    HDC hdc = GetDC(nullptr);
    UINT dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    return dpi ? dpi : 96;
}

int Theme::Scale(int val, HWND hwnd) const {
    UINT dpi = GetDpi(hwnd);
    return MulDiv(val, dpi, 96);
}

COLORREF Theme::ClrBackground() const {
    return m_isDark ? RGB(32, 32, 32) : RGB(255, 255, 255);
}

COLORREF Theme::ClrSurface() const {
    return m_isDark ? RGB(40, 40, 40) : RGB(243, 243, 243);
}

COLORREF Theme::ClrHover() const {
    return m_isDark ? RGB(55, 55, 55) : RGB(232, 232, 232);
}

COLORREF Theme::ClrSelected() const {
    return m_isDark ? RGB(65, 65, 65) : RGB(204, 232, 255);
}

COLORREF Theme::ClrBorder() const {
    return m_isDark ? RGB(58, 58, 58) : RGB(218, 218, 218);
}

COLORREF Theme::ClrTextPrimary() const {
    return m_isDark ? RGB(245, 245, 245) : RGB(25, 25, 25);
}

COLORREF Theme::ClrTextSecondary() const {
    return m_isDark ? RGB(165, 165, 165) : RGB(110, 110, 110);
}

COLORREF Theme::ClrAccent() const {
    int mode = Config::Get().accentColorMode;
    switch (mode) {
    case 1: // Emerald Teal
        return m_isDark ? RGB(80, 227, 194) : RGB(16, 137, 62);
    case 2: // Fluent Purple
        return m_isDark ? RGB(180, 140, 255) : RGB(112, 60, 216);
    case 3: // Sunset Orange
        return m_isDark ? RGB(255, 155, 80) : RGB(232, 90, 20);
    case 4: // Crimson Red
        return m_isDark ? RGB(255, 110, 120) : RGB(216, 40, 60);
    case 5: // Monochrome Slate
        return m_isDark ? RGB(190, 195, 205) : RGB(70, 75, 85);
    case 6: // Custom
        return Config::Get().customAccentColor;
    case 0: // Default Neo Blue
    default:
        return m_isDark ? RGB(96, 205, 255) : RGB(0, 120, 212);
    }
}

void Theme::RecreateGdiObjects() {
    if (m_hbrBg) DeleteObject(m_hbrBg);
    if (m_hbrSurface) DeleteObject(m_hbrSurface);
    if (m_hbrHover) DeleteObject(m_hbrHover);
    if (m_hbrSelected) DeleteObject(m_hbrSelected);
    if (m_hbrBorder) DeleteObject(m_hbrBorder);
    if (m_hbrAccent) DeleteObject(m_hbrAccent);
    if (m_hpenBorder) DeleteObject(m_hpenBorder);

    m_hbrBg = CreateSolidBrush(ClrBackground());
    m_hbrSurface = CreateSolidBrush(ClrSurface());
    m_hbrHover = CreateSolidBrush(ClrHover());
    m_hbrSelected = CreateSolidBrush(ClrSelected());
    m_hbrBorder = CreateSolidBrush(ClrBorder());
    m_hbrAccent = CreateSolidBrush(ClrAccent());
    m_hpenBorder = CreatePen(PS_SOLID, 1, ClrBorder());
}

HFONT Theme::GetUIFont(HWND hwnd) {
    int currentDpi = (int)GetDpi(hwnd);
    if (!m_fontUI || m_lastFontDpi != currentDpi) {
        if (m_fontUI) DeleteObject(m_fontUI);
        if (m_fontUIBold) DeleteObject(m_fontUIBold);

        m_lastFontDpi = currentDpi;
        int fontSize = -MulDiv(9, currentDpi, 72);

        LOGFONTW lf = { 0 };
        if (SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(LOGFONTW), &lf, 0)) {
            lf.lfHeight = fontSize;
            lf.lfWeight = FW_NORMAL;
            lf.lfQuality = CLEARTYPE_QUALITY;
        } else {
            lf.lfHeight = fontSize;
            lf.lfWeight = FW_NORMAL;
            lf.lfQuality = CLEARTYPE_QUALITY;
            StringCchCopyW(lf.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
        }

        m_fontUI = CreateFontIndirectW(&lf);

        lf.lfWeight = FW_SEMIBOLD;
        m_fontUIBold = CreateFontIndirectW(&lf);
    }
    return m_fontUI;
}

HFONT Theme::GetUIFontBold(HWND hwnd) {
    GetUIFont(hwnd);
    return m_fontUIBold;
}

HFONT Theme::GetIconFont(int pixelHeight, HWND hwnd) {
    int scaledH = Scale(pixelHeight, hwnd);
    LOGFONTW lf = { 0 };
    lf.lfHeight = -scaledH;
    lf.lfWeight = FW_NORMAL;
    lf.lfQuality = CLEARTYPE_QUALITY;
    // Prefer Windows 11 Fluent icons, fallback to Win10 Segoe MDL2 Assets, then Segoe UI Symbol
    StringCchCopyW(lf.lfFaceName, LF_FACESIZE, L"Segoe Fluent Icons");
    HFONT hFont = CreateFontIndirectW(&lf);
    return hFont;
}

void Theme::DrawSmoothRoundRect(HDC hdc, const RECT& rc, int radius, COLORREF fillColor, COLORREF borderColor, float borderWidth, BYTE fillAlpha) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    Gdiplus::Graphics g(hdc);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    bool hasBorder = (borderColor != CLR_INVALID && borderWidth > 0.0f);
    float inset = hasBorder ? (borderWidth * 0.5f) : 0.0f;
    float x = (float)rc.left + inset;
    float y = (float)rc.top + inset;
    float fw = (float)w - (hasBorder ? borderWidth : 0.0f);
    float fh = (float)h - (hasBorder ? borderWidth : 0.0f);
    float d = (float)(radius * 2) - (hasBorder ? borderWidth : 0.0f);

    if (d > fw) d = fw;
    if (d > fh) d = fh;
    if (d < 1.0f) d = 1.0f;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + fw - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + fw - d, y + fh - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + fh - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();

    if (fillColor != CLR_INVALID) {
        Gdiplus::Color gdiFillColor(fillAlpha, GetRValue(fillColor), GetGValue(fillColor), GetBValue(fillColor));
        Gdiplus::SolidBrush brush(gdiFillColor);
        g.FillPath(&brush, &path);
    }

    if (hasBorder) {
        Gdiplus::Color gdiBorderColor(GetRValue(borderColor), GetGValue(borderColor), GetBValue(borderColor));
        Gdiplus::Pen pen(gdiBorderColor, borderWidth);
        g.DrawPath(&pen, &path);
    }
}

void Theme::FillSmoothRoundRect(HDC hdc, const RECT& rc, int radius, COLORREF fillColor, BYTE fillAlpha) {
    DrawSmoothRoundRect(hdc, rc, radius, fillColor, CLR_INVALID, 0.0f, fillAlpha);
}

