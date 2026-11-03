#pragma once
#include "..\Common.h"

struct BreadcrumbSegment {
    std::wstring name;
    std::wstring targetPath;
    RECT rc = { 0 };
};

class BreadcrumbBar {
public:
    BreadcrumbBar();
    ~BreadcrumbBar();

    HWND Create(HWND hwndParent, int id);
    HWND GetHwnd() const { return m_hwnd; }

    void SetPath(const std::wstring& path);
    std::wstring GetPath() const { return m_currentPath; }

    void UpdateLayout(int x, int y, int width, int height);

    std::function<void(const std::wstring& path)> onPathNavigated;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Draw(HDC hdc);
    void ParseSegments();
    void SwitchToEditMode();
    void CommitEdit();
    void CancelEdit();

    HWND m_hwnd = nullptr;
    HWND m_hwndEdit = nullptr;
    std::wstring m_currentPath;
    std::vector<BreadcrumbSegment> m_segments;
    int m_hoverSegment = -1;
    bool m_editMode = false;
};
