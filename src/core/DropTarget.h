#pragma once
#include "..\Common.h"
#include <oleidl.h>

class DropTarget : public IDropTarget {
public:
    DropTarget(HWND hwnd, std::function<std::wstring(POINT ptScreen, bool& outIsSubFolder)> targetDirCallback);
    virtual ~DropTarget();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IDropTarget
    STDMETHODIMP DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    STDMETHODIMP DragLeave() override;
    STDMETHODIMP Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;

    std::function<void(const std::wstring& targetDir, const std::vector<std::wstring>& files, bool isMove)> onFilesDropped;
    std::function<void(POINTL pt)> onDragHover;
    std::function<void()> onDragLeave;

private:
    bool QueryDropFiles(IDataObject* pDataObj, std::vector<std::wstring>& outFiles);

    LONG m_refCount = 1;
    HWND m_hwnd = nullptr;
    bool m_acceptable = false;
    std::vector<std::wstring> m_cachedFiles;
    std::function<std::wstring(POINT ptScreen, bool& outIsSubFolder)> m_targetDirCallback;
};
