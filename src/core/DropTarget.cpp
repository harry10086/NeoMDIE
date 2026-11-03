#include "DropTarget.h"

DropTarget::DropTarget(HWND hwnd, std::function<std::wstring(POINT ptScreen, bool& outIsSubFolder)> targetDirCallback)
    : m_hwnd(hwnd), m_targetDirCallback(targetDirCallback) {
}

DropTarget::~DropTarget() {
}

STDMETHODIMP DropTarget::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDropTarget) {
        *ppv = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) DropTarget::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) DropTarget::Release() {
    LONG res = InterlockedDecrement(&m_refCount);
    if (res == 0) {
        delete this;
    }
    return res;
}

bool DropTarget::QueryDropFiles(IDataObject* pDataObj, std::vector<std::wstring>& outFiles) {
    if (!pDataObj) return false;

    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM med;
    if (SUCCEEDED(pDataObj->GetData(&fmt, &med))) {
        HDROP hDrop = (HDROP)GlobalLock(med.hGlobal);
        if (hDrop) {
            UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < count; ++i) {
                wchar_t buf[MAX_PATH];
                if (DragQueryFileW(hDrop, i, buf, MAX_PATH)) {
                    outFiles.push_back(buf);
                }
            }
            GlobalUnlock(med.hGlobal);
        }
        ReleaseStgMedium(&med);
        return !outFiles.empty();
    }
    return false;
}

STDMETHODIMP DropTarget::DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    if (!pdwEffect) return E_POINTER;

    std::vector<std::wstring> files;
    m_acceptable = QueryDropFiles(pDataObj, files);

    if (m_acceptable) {
        bool isMove = (grfKeyState & MK_SHIFT) != 0;
        *pdwEffect = isMove ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
    } else {
        *pdwEffect = DROPEFFECT_NONE;
    }

    if (onDragHover) {
        onDragHover(pt);
    }
    return S_OK;
}

STDMETHODIMP DropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    if (!pdwEffect) return E_POINTER;

    if (m_acceptable) {
        bool isMove = (grfKeyState & MK_SHIFT) != 0;
        *pdwEffect = isMove ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
    } else {
        *pdwEffect = DROPEFFECT_NONE;
    }

    if (onDragHover) {
        onDragHover(pt);
    }
    return S_OK;
}

STDMETHODIMP DropTarget::DragLeave() {
    m_acceptable = false;
    if (onDragLeave) {
        onDragLeave();
    }
    return S_OK;
}

STDMETHODIMP DropTarget::Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    if (!pdwEffect) return E_POINTER;

    if (!m_acceptable) {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    std::vector<std::wstring> files;
    if (QueryDropFiles(pDataObj, files)) {
        bool isMove = (grfKeyState & MK_SHIFT) != 0;
        *pdwEffect = isMove ? DROPEFFECT_MOVE : DROPEFFECT_COPY;

        POINT ptScreen = { pt.x, pt.y };
        bool isSub = false;
        std::wstring targetDir = m_targetDirCallback ? m_targetDirCallback(ptScreen, isSub) : L"";

        if (!targetDir.empty() && onFilesDropped) {
            onFilesDropped(targetDir, files, isMove);
        }
    }

    if (onDragLeave) {
        onDragLeave();
    }
    return S_OK;
}
