#pragma once
#include "..\Common.h"

struct LinkItem {
    std::wstring name;
    std::wstring path;
    COLORREF color = 0; // 0 = default, otherwise custom RGB
};

class LinksBar {
public:
    LinksBar();
    ~LinksBar();

    HWND Create(HWND hwndParent, int id);
    HWND GetHwnd() const { return m_hwnd; }

    void LoadLinks();
    void SaveLinks();
    void AddLink(const std::wstring& name, const std::wstring& path);
    void RemoveLink(int index);
    void SetLinkColor(int index, COLORREF color);
    const std::vector<LinkItem>& GetLinks() const { return m_links; }

    void UpdateLayout(int x, int y, int width, int height);

    std::function<void(const std::wstring& path)> onNavigate;
    std::function<void(const std::wstring& path)> onOpenNewTab;
    std::function<void(const std::wstring& path)> onOpenBgTab;
    std::function<void(const std::wstring& path)> onOpenSplit;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void UpdateTooltip(int index);

    HWND m_hwnd = nullptr;
    HWND m_hwndTooltip = nullptr;
    std::vector<LinkItem> m_links;
    std::vector<RECT> m_itemRects;
    int m_hoverIndex = -1;
    int m_pressedIndex = -1;
    RECT m_rcIcon = { 0 };
    bool m_hoverIcon = false;
    bool m_dragging = false;
    bool m_dragPotential = false;
    int m_dragIndex = -1;
    POINT m_dragStart = { 0, 0 };
    COLORREF m_customColors[16] = { 0 };

    int m_scrollOffset = 0;
    int m_maxScroll = 0;
    bool m_showScrollButtons = false;
    RECT m_rcScrollLeft = { 0 };
    RECT m_rcScrollRight = { 0 };
    bool m_hoverScrollLeft = false;
    bool m_hoverScrollRight = false;
    void ScrollBy(int delta);
};
