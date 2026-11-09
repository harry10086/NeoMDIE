#pragma once
#include "..\Common.h"

enum class GestureAction {
    None = 0,
    NavBack = 1,
    NavForward = 2,
    NavUp = 3,
    CloseTab = 4,
    NewTab = 5,
    Refresh = 6,
    DuplicateTab = 7,
    ToggleTree = 8,
    TogglePreview = 9,
    OpenTerminal = 10,
    MaximizePane = 11,
    ClosePane = 12,
    ScrollTop = 13,
    ScrollBottom = 14,
    PageUp = 15,
    PageDown = 16,
    NewFolder = 17,
    Copy = 18,
    Cut = 19,
    Paste = 20,
    SelectAll = 21,
    Properties = 22,
    LockTab = 23,
    ReopenClosedTab = 24,
    NextTab = 25,
    PrevTab = 26,
    CloseOtherTabs = 27,
    LayoutSingle = 28,
    Layout2H = 29,
    Layout2V = 30,
    Layout4Grid = 31,
    CopyPath = 32,
    Delete = 33,
    Rename = 34,
    NewMD = 35,
    ToggleHidden = 36,
    OpenSettings = 37,
    InvertSelection = 38,
    Undo = 39
};

class GestureEngine {
public:
    GestureEngine();
    ~GestureEngine();

    static std::wstring GetActionName(GestureAction action);
    static std::wstring CodeToArrowString(const std::string& code);

    void Attach(HWND hTargetWnd);
    void Detach();

    void OnRButtonDown(HWND hwnd, int x, int y);
    bool OnMouseMove(HWND hwnd, int x, int y, bool isRButtonDown);
    bool OnRButtonUp(HWND hwnd, int x, int y, GestureAction& outAction);

    bool IsGestureActive() const { return m_isDragging; }
    std::wstring GetCurrentGestureName() const;

private:
    void ProcessMovement(int x, int y);
    GestureAction MatchGesture() const;
    void ShowHud(const POINT& ptScreen);
    void UpdateHudText();
    void HideHud();

    HWND m_hTargetWnd = nullptr;
    HWND m_hHudWnd = nullptr;

    bool m_isRDown = false;
    bool m_isDragging = false;
    POINT m_startPoint = { 0, 0 };
    POINT m_lastVectorPoint = { 0, 0 };
    std::vector<POINT> m_points;
    std::string m_gestureCode; // e.g. "L", "R", "U", "D"
};
