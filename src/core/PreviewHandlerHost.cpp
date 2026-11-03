#include "PreviewHandlerHost.h"
#include "I18n.h"
#include "..\ui\Theme.h"
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")

static const wchar_t* PREVIEW_HANDLER_GUID = L"{8895b1c6-b41f-4c1c-a562-0d564250836f}";

PreviewHandlerHost::PreviewHandlerHost() {
}

PreviewHandlerHost::~PreviewHandlerHost() {
    Unload();
    Detach();
}

bool PreviewHandlerHost::Attach(HWND hwndContainer) {
    m_hwndContainer = hwndContainer;
    return true;
}

void PreviewHandlerHost::Detach() {
    Unload();
    m_hwndContainer = nullptr;
}

bool PreviewHandlerHost::QueryPreviewHandlerClsid(const std::wstring& ext, CLSID& outClsid) {
    if (ext.empty()) return false;

    wchar_t clsidStr[64] = { 0 };
    DWORD size = sizeof(clsidStr);

    // 1. Direct extension shellex key: HKCR\<ext>\shellex\{8895b1c6-b41f-4c1c-a562-0d564250836f}
    std::wstring directKey = ext + L"\\shellex\\" + PREVIEW_HANDLER_GUID;
    if (RegGetValueW(HKEY_CLASSES_ROOT, directKey.c_str(), nullptr, RRF_RT_REG_SZ,
                     nullptr, clsidStr, &size) == ERROR_SUCCESS) {
        return SUCCEEDED(CLSIDFromString(clsidStr, &outClsid));
    }

    // 2. ProgID shellex key: HKCR\<ProgID>\shellex\{8895b1c6-b41f-4c1c-a562-0d564250836f}
    wchar_t progId[128] = { 0 };
    DWORD progIdSize = sizeof(progId);
    if (RegGetValueW(HKEY_CLASSES_ROOT, ext.c_str(), nullptr, RRF_RT_REG_SZ,
                     nullptr, progId, &progIdSize) == ERROR_SUCCESS) {
        std::wstring progIdKey = std::wstring(progId) + L"\\shellex\\" + PREVIEW_HANDLER_GUID;
        size = sizeof(clsidStr);
        if (RegGetValueW(HKEY_CLASSES_ROOT, progIdKey.c_str(), nullptr, RRF_RT_REG_SZ,
                         nullptr, clsidStr, &size) == ERROR_SUCCESS) {
            return SUCCEEDED(CLSIDFromString(clsidStr, &outClsid));
        }
    }

    return false;
}

void PreviewHandlerHost::Unload() {
    if (m_pPreviewHandler) {
        m_pPreviewHandler->Unload();
        m_pPreviewHandler->Release();
        m_pPreviewHandler = nullptr;
    }
    if (m_hFallbackBitmap) {
        DeleteObject(m_hFallbackBitmap);
        m_hFallbackBitmap = nullptr;
    }
    m_fallbackText.clear();
    m_currentFile.clear();
}

bool PreviewHandlerHost::LoadFile(const std::wstring& filePath) {
    Unload();

    if (filePath.empty() || !PathFileExistsW(filePath.c_str()) || PathIsDirectoryW(filePath.c_str())) {
        return false;
    }

    m_currentFile = filePath;
    const wchar_t* pExt = PathFindExtensionW(filePath.c_str());
    std::wstring ext = pExt ? pExt : L"";

    RECT rc = { 0 };
    if (m_hwndContainer) {
        GetClientRect(m_hwndContainer, &rc);
    }

    // Try Windows native IPreviewHandler
    CLSID clsid;
    if (QueryPreviewHandlerClsid(ext, clsid)) {
        HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER,
                                      IID_IPreviewHandler, (void**)&m_pPreviewHandler);
        if (SUCCEEDED(hr) && m_pPreviewHandler) {
            bool initialized = false;

            // Try IInitializeWithFile
            IInitializeWithFile* pInitFile = nullptr;
            if (SUCCEEDED(m_pPreviewHandler->QueryInterface(IID_PPV_ARGS(&pInitFile)))) {
                if (SUCCEEDED(pInitFile->Initialize(filePath.c_str(), STGM_READ))) {
                    initialized = true;
                }
                pInitFile->Release();
            }

            // Try IInitializeWithItem
            if (!initialized) {
                IInitializeWithItem* pInitItem = nullptr;
                if (SUCCEEDED(m_pPreviewHandler->QueryInterface(IID_PPV_ARGS(&pInitItem)))) {
                    IShellItem* pShellItem = nullptr;
                    if (SUCCEEDED(SHCreateItemFromParsingName(filePath.c_str(), nullptr, IID_PPV_ARGS(&pShellItem)))) {
                        if (SUCCEEDED(pInitItem->Initialize(pShellItem, STGM_READ))) {
                            initialized = true;
                        }
                        pShellItem->Release();
                    }
                    pInitItem->Release();
                }
            }

            if (initialized) {
                m_pPreviewHandler->SetWindow(m_hwndContainer, &rc);
                if (SUCCEEDED(m_pPreviewHandler->DoPreview())) {
                    return true;
                }
            }

            m_pPreviewHandler->Unload();
            m_pPreviewHandler->Release();
            m_pPreviewHandler = nullptr;
        }
    }

    // Fallback 1: Text file preview (first 64KB)
    std::wstring lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::towlower);
    if (lowerExt == L".txt" || lowerExt == L".md" || lowerExt == L".json" || lowerExt == L".ini" ||
        lowerExt == L".cpp" || lowerExt == L".h" || lowerExt == L".c" || lowerExt == L".py" ||
        lowerExt == L".js" || lowerExt == L".log" || lowerExt == L".bat" || lowerExt == L".xml" ||
        lowerExt == L".html" || lowerExt == L".css" || lowerExt == L".cmd" || lowerExt == L".svg") {
        
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            char buffer[65536] = { 0 };
            DWORD bytesRead = 0;
            ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
            CloseHandle(hFile);

            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                m_fallbackText = Util::Utf8ToWide(buffer);
                if (m_fallbackText.empty()) {
                    // Try ANSI if UTF8 conversion failed
                    int wlen = MultiByteToWideChar(CP_ACP, 0, buffer, bytesRead, nullptr, 0);
                    m_fallbackText.resize(wlen);
                    MultiByteToWideChar(CP_ACP, 0, buffer, bytesRead, &m_fallbackText[0], wlen);
                }
            }
        }
    }

    return true;
}

void PreviewHandlerHost::Resize(const RECT& rc) {
    if (m_pPreviewHandler) {
        m_pPreviewHandler->SetRect(&rc);
    }
}

void PreviewHandlerHost::DrawFallback(HDC hdc, const RECT& rc) {
    if (m_pPreviewHandler) return; // Handled by active Preview Handler

    SetBkMode(hdc, TRANSPARENT);

    if (!m_fallbackText.empty()) {
        HFONT hFont = Theme::Get().GetUIFont(m_hwndContainer);
        HGDIOBJ oldFont = SelectObject(hdc, hFont);
        SetTextColor(hdc, Theme::Get().ClrTextPrimary());

        RECT textRc = rc;
        InflateRect(&textRc, -12, -12);
        DrawTextW(hdc, m_fallbackText.c_str(), -1, &textRc,
                  DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EXPANDTABS | DT_NOPREFIX);

        SelectObject(hdc, oldFont);
    } else {
        // Generic file info
        HFONT hFontBold = Theme::Get().GetUIFontBold(m_hwndContainer);
        HGDIOBJ oldFont = SelectObject(hdc, hFontBold);
        SetTextColor(hdc, Theme::Get().ClrTextSecondary());

        std::wstring msg = m_currentFile.empty() ? 
            I18n::Get().T(StringId::PreviewSelectHint) : 
            I18n::Get().T(StringId::PreviewNoHandler);
        RECT centerRc = rc;
        DrawTextW(hdc, msg.c_str(), -1, &centerRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
    }
}
