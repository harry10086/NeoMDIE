#pragma once
#include "..\Common.h"
#include "FileListView.h"

class SplitContainer {
public:
    SplitContainer();
    ~SplitContainer();

    HWND Create(HWND hwndParent, int id);
    HWND GetHwnd() const { return m_hwnd; }

    void SetSplitEnabled(bool enabled);
    bool IsSplitEnabled() const { return m_splitEnabled; }

    void SetSplitRatio(int ratio); // 10 to 90
    int GetSplitRatio() const { return m_splitRatio; }

    FileListView& GetLeftView() { return m_leftView; }
    FileListView& GetRightView() { return m_rightView; }
    FileListView* GetActiveView() { return m_activeRight ? &m_rightView : &m_leftView; }
    void SetActiveView(bool right) { m_activeRight = right; }

    void UpdateLayout(int x, int y, int width, int height);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void LayoutChildren();

    HWND m_hwnd = nullptr;
    FileListView m_leftView;
    FileListView m_rightView;

    bool m_splitEnabled = false;
    int  m_splitRatio = 50; // percentage
    bool m_activeRight = false;

    bool m_draggingSplitter = false;
    RECT m_rcSplitter = { 0 };
};
