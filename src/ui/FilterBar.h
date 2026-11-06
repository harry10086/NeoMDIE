#pragma once
#include "..\Common.h"

class FilterBar {
public:
    FilterBar();
    ~FilterBar();

    HWND Create(HWND hwndParent, int id);
    HWND GetHwnd() const { return m_hwnd; }

    void Clear();
    void SetText(const std::wstring& text);
    void SetFocus();
    void UpdateLanguage();
    std::wstring GetText() const;
    void UpdateLayout(int x, int y, int width, int height);

    bool IsSearchSubfolders() const { return m_searchSubfolders; }
    bool IsMatchCase() const { return m_matchCase; }

    std::function<void(const std::wstring& query, bool subfolders, bool matchCase)> onFilterChanged;
    std::function<void()> onEscapePressed;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    void ShowOptionsMenu();

    HWND m_hwnd = nullptr;
    HWND m_hwndEdit = nullptr;
    HWND m_hwndOptBtn = nullptr;

    bool m_searchSubfolders = false;
    bool m_matchCase = false;
    bool m_suppressNotify = false;
};
