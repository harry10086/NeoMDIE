#include "DropTarget.h"
#include "I18n.h"
#include "ShellUtils.h"

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

static bool HasVirtualDropFiles(IDataObject* pDataObj) {
    if (!pDataObj) return false;
    static UINT s_uFmtDescriptorW = RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW);
    static UINT s_uFmtDescriptorA = RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORA);
    FORMATETC fmt = { (CLIPFORMAT)s_uFmtDescriptorW, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    if (SUCCEEDED(pDataObj->QueryGetData(&fmt))) return true;
    fmt.cfFormat = (CLIPFORMAT)s_uFmtDescriptorA;
    if (SUCCEEDED(pDataObj->QueryGetData(&fmt))) return true;
    return false;
}

STDMETHODIMP DropTarget::DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    if (!pdwEffect) return E_POINTER;

    m_cachedFiles.clear();
    m_acceptable = QueryDropFiles(pDataObj, m_cachedFiles) || HasVirtualDropFiles(pDataObj);

    if (m_acceptable) {
        if (grfKeyState & MK_CONTROL) {
            *pdwEffect = DROPEFFECT_COPY;
        } else if (grfKeyState & MK_SHIFT) {
            *pdwEffect = DROPEFFECT_MOVE;
        } else if (grfKeyState & MK_ALT) {
            *pdwEffect = DROPEFFECT_LINK;
        } else {
            // Default effect: same volume = MOVE, different volume = COPY
            POINT ptScreen = { pt.x, pt.y };
            bool isSub = false;
            std::wstring targetDir = m_targetDirCallback ? m_targetDirCallback(ptScreen, isSub) : L"";
            if (!m_cachedFiles.empty() && !targetDir.empty() &&
                m_cachedFiles[0].length() > 1 && m_cachedFiles[0][1] == L':' &&
                targetDir.length() > 1 && targetDir[1] == L':' &&
                towupper(m_cachedFiles[0][0]) == towupper(targetDir[0])) {
                *pdwEffect = DROPEFFECT_MOVE;
            } else {
                *pdwEffect = DROPEFFECT_COPY;
            }
        }
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
        if (grfKeyState & MK_CONTROL) {
            *pdwEffect = DROPEFFECT_COPY;
        } else if (grfKeyState & MK_SHIFT) {
            *pdwEffect = DROPEFFECT_MOVE;
        } else if (grfKeyState & MK_ALT) {
            *pdwEffect = DROPEFFECT_LINK;
        } else {
            POINT ptScreen = { pt.x, pt.y };
            bool isSub = false;
            std::wstring targetDir = m_targetDirCallback ? m_targetDirCallback(ptScreen, isSub) : L"";
            if (!m_cachedFiles.empty() && !targetDir.empty() &&
                m_cachedFiles[0].length() > 1 && m_cachedFiles[0][1] == L':' &&
                targetDir.length() > 1 && targetDir[1] == L':' &&
                towupper(m_cachedFiles[0][0]) == towupper(targetDir[0])) {
                *pdwEffect = DROPEFFECT_MOVE;
            } else {
                *pdwEffect = DROPEFFECT_COPY;
            }
        }
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
    m_cachedFiles.clear();
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

    std::vector<std::wstring> files = m_cachedFiles;
    if (files.empty()) {
        QueryDropFiles(pDataObj, files);
    }

    POINT ptScreen = { pt.x, pt.y };
    bool isSub = false;
    std::wstring targetDir = m_targetDirCallback ? m_targetDirCallback(ptScreen, isSub) : L"";

    if (!files.empty() && !targetDir.empty()) {
        bool isMove = false;
        bool isLink = false;

        if (grfKeyState & MK_CONTROL) {
            isMove = false;
            *pdwEffect = DROPEFFECT_COPY;
        } else if (grfKeyState & MK_SHIFT) {
            isMove = true;
            *pdwEffect = DROPEFFECT_MOVE;
        } else if (grfKeyState & MK_ALT) {
            isLink = true;
            *pdwEffect = DROPEFFECT_LINK;
        } else {
            // Same volume = MOVE, different volume = COPY
            if (files[0].length() > 1 && files[0][1] == L':' &&
                targetDir.length() > 1 && targetDir[1] == L':' &&
                towupper(files[0][0]) == towupper(targetDir[0])) {
                isMove = true;
                *pdwEffect = DROPEFFECT_MOVE;
            } else {
                isMove = false;
                *pdwEffect = DROPEFFECT_COPY;
            }
        }

        if (isLink) {
            for (const auto& f : files) {
                std::wstring fn = PathFindFileNameW(f.c_str());
                std::wstring linkPath = targetDir;
                if (linkPath.back() != L'\\') linkPath += L'\\';
                linkPath += fn + (I18n::Get().IsChinese() ? L" - 快捷方式.lnk" : L" - Shortcut.lnk");
                ShellUtils::CreateShortcut(linkPath, f);
            }
            SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, targetDir.c_str(), nullptr);
            if (onFilesDropped) {
                onFilesDropped(targetDir, files, false);
            }
        } else if (onFilesDropped) {
            onFilesDropped(targetDir, files, isMove);
        }
    } else if (HasVirtualDropFiles(pDataObj)) {
        POINT ptScreen = { pt.x, pt.y };
        bool isSub = false;
        std::wstring targetDir = m_targetDirCallback ? m_targetDirCallback(ptScreen, isSub) : L"";
        if (!targetDir.empty()) {
            IShellItem* psiDest = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(targetDir.c_str(), nullptr, IID_PPV_ARGS(&psiDest))) && psiDest) {
                IDropTarget* pShellDropTarget = nullptr;
                if (SUCCEEDED(psiDest->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&pShellDropTarget))) && pShellDropTarget) {
                    DWORD dwEffect = DROPEFFECT_COPY;
                    if (SUCCEEDED(pShellDropTarget->DragEnter(pDataObj, grfKeyState, pt, &dwEffect)) && dwEffect != DROPEFFECT_NONE) {
                        pShellDropTarget->Drop(pDataObj, grfKeyState, pt, &dwEffect);
                        *pdwEffect = dwEffect;
                    } else {
                        pShellDropTarget->DragLeave();
                    }
                    pShellDropTarget->Release();
                }
                psiDest->Release();
            }
        }
    }

    m_cachedFiles.clear();
    if (onDragLeave) {
        onDragLeave();
    }
    return S_OK;
}
