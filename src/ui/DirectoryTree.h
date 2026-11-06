#pragma once
#include "..\Common.h"

struct TreeNodeData {
    std::wstring path;
    bool isDrive = false;
    bool childrenLoaded = false;
};

class DirectoryTree {
public:
    DirectoryTree();
    ~DirectoryTree();

    HWND Create(HWND hwndParent, int id);
    HWND GetHwnd() const { return m_hwnd; }

    void Populate();
    void Refresh();
    void SelectPath(const std::wstring& path);
    void ApplyTheme();

    void UpdateLayout(int x, int y, int width, int height);

    std::function<void(const std::wstring& path)> onPathSelected;
    std::function<void(int cmd, const std::wstring& path)> onCustomCommand;

private:
    static LRESULT CALLBACK ParentSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    static LRESULT CALLBACK TreeSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    void PopulateQuickAccess(HTREEITEM hParent);
    void PopulateThisPC(HTREEITEM hParent);
    void PopulateChildren(HTREEITEM hParent, const std::wstring& folderPath);
    void AddSpecialFolder(HTREEITEM hParent, const wchar_t* name, int csidl);
    void FreeAllNodeData();
    HTREEITEM FindOrLoadChildPath(HTREEITEM hParent, const std::wstring& targetPath);

    HWND m_hwndParent = nullptr;
    HWND m_hwnd = nullptr;
    int m_controlId = 0;
    bool m_syncing = false;

    HTREEITEM m_hQuickAccess = nullptr;
    HTREEITEM m_hThisPC = nullptr;
    HTREEITEM m_hDropHighlight = nullptr;

    class DropTarget* m_pDropTarget = nullptr;
    void StartDragDrop(HTREEITEM hItem);
    std::vector<TreeNodeData*> m_nodeDataList;
};
