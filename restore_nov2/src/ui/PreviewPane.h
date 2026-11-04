#pragma once
#include "..\Common.h"
#include "..\core\PreviewHandlerHost.h"

class PreviewPane {
public:
    PreviewPane();
    ~PreviewPane();

    HWND Create(HWND hwndParent, int id);
    HWND GetHwnd() const { return m_hwnd; }

    void SetFile(const std::wstring& filePath);
    void Clear();
    void UpdateLayout(int x, int y, int width, int height);
    void ApplyTheme();

    std::function<void()> onCloseRequested;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    PreviewHandlerHost m_host;
    std::wstring m_currentFile;
    bool m_hoverClose = false;
    RECT m_rcClose = { 0 };
};
