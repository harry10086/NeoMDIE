#pragma once
#include "..\Common.h"

class DropIndicatorOverlay {
public:
    DropIndicatorOverlay();
    ~DropIndicatorOverlay();

    void Create(HWND hwndOwner);
    void Show(const RECT& targetRect, const std::wstring& text);
    void Hide();
    bool IsVisible() const { return m_visible; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    bool m_visible = false;
    RECT m_currentRect = { 0, 0, 0, 0 };
    std::wstring m_text;
};

