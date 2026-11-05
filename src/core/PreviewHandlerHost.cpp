#include "PreviewHandlerHost.h"
#include "I18n.h"
#include "..\ui\Theme.h"
#include <wincodec.h>
#include <shlwapi.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "msimg32.lib")

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
    m_imgWidth = 0;
    m_imgHeight = 0;
    m_imgInfoText.clear();
    m_isPdf = false;
    m_fallbackText.clear();
    m_currentFile.clear();
}

bool PreviewHandlerHost::LoadImagePreview(const std::wstring& filePath) {
    IWICImagingFactory* pFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pFactory));
    if (FAILED(hr) || !pFactory) return false;

    IWICBitmapDecoder* pDecoder = nullptr;
    hr = pFactory->CreateDecoderFromFilename(filePath.c_str(), nullptr, GENERIC_READ,
                                            WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr) || !pDecoder) {
        pFactory->Release();
        return false;
    }

    IWICBitmapFrameDecode* pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr) || !pFrame) {
        pDecoder->Release();
        pFactory->Release();
        return false;
    }

    IWICFormatConverter* pConverter = nullptr;
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr) || !pConverter) {
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        return false;
    }

    hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA,
                                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        pConverter->Release();
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        return false;
    }

    UINT width = 0, height = 0;
    pConverter->GetSize(&width, &height);
    if (width == 0 || height == 0) {
        pConverter->Release();
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        return false;
    }

    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = (LONG)width;
    bi.bmiHeader.biHeight = -(LONG)height; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HBITMAP hDIB = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    ReleaseDC(nullptr, hdcScreen);

    if (hDIB && pBits) {
        UINT stride = width * 4;
        UINT bufferSize = stride * height;
        hr = pConverter->CopyPixels(nullptr, stride, bufferSize, (BYTE*)pBits);
        if (SUCCEEDED(hr)) {
            m_hFallbackBitmap = hDIB;
            m_imgWidth = (int)width;
            m_imgHeight = (int)height;

            uint64_t fileSize = 0;
            WIN32_FILE_ATTRIBUTE_DATA fad;
            if (GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &fad)) {
                ULARGE_INTEGER uli;
                uli.LowPart = fad.nFileSizeLow;
                uli.HighPart = fad.nFileSizeHigh;
                fileSize = uli.QuadPart;
            }

            const wchar_t* pExt = PathFindExtensionW(filePath.c_str());
            std::wstring extStr = pExt ? (pExt + 1) : L"";
            std::transform(extStr.begin(), extStr.end(), extStr.begin(), ::towupper);

            wchar_t infoBuf[128];
            StringCchPrintfW(infoBuf, 128, L"%d × %d  ·  %s  ·  %s",
                             m_imgWidth, m_imgHeight, extStr.c_str(), Util::FormatBytes(fileSize).c_str());
            m_imgInfoText = infoBuf;
        } else {
            DeleteObject(hDIB);
        }
    }

    pConverter->Release();
    pFrame->Release();
    pDecoder->Release();
    pFactory->Release();
    return (m_hFallbackBitmap != nullptr);
}

bool PreviewHandlerHost::LoadFile(const std::wstring& filePath) {
    Unload();

    if (filePath.empty() || !PathFileExistsW(filePath.c_str()) || PathIsDirectoryW(filePath.c_str())) {
        return false;
    }

    m_currentFile = filePath;
    const wchar_t* pExt = PathFindExtensionW(filePath.c_str());
    std::wstring ext = pExt ? pExt : L"";
    std::wstring lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::towlower);

    // 1. Direct high-performance WIC Image Preview (JPG, PNG, GIF, BMP, WEBP, ICO, TIFF etc.)
    if (lowerExt == L".jpg" || lowerExt == L".jpeg" || lowerExt == L".png" || lowerExt == L".bmp" ||
        lowerExt == L".gif" || lowerExt == L".ico" || lowerExt == L".webp" || lowerExt == L".tif" ||
        lowerExt == L".tiff" || lowerExt == L".jfif" || lowerExt == L".cur" || lowerExt == L".dib") {
        if (LoadImagePreview(filePath)) {
            return true;
        }
    }

    // Calculate content preview rectangle excluding top header
    RECT rc = { 0 };
    if (m_hwndContainer) {
        GetClientRect(m_hwndContainer, &rc);
    }
    int headerH = Theme::Get().Scale(26, m_hwndContainer);
    RECT rcPreview = { 0, headerH, rc.right, rc.bottom };

    // 2. Try Windows native COM IPreviewHandler (PDF, Office, etc.)
    CLSID clsid;
    if (QueryPreviewHandlerClsid(ext, clsid)) {
        HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER,
                                      IID_IPreviewHandler, (void**)&m_pPreviewHandler);
        if (SUCCEEDED(hr) && m_pPreviewHandler) {
            bool initialized = false;

            // (1) Try IInitializeWithStream (Standard official way for PDF / Edge / Acrobat)
            IInitializeWithStream* pInitStream = nullptr;
            if (SUCCEEDED(m_pPreviewHandler->QueryInterface(IID_PPV_ARGS(&pInitStream)))) {
                IStream* pStream = nullptr;
                if (SUCCEEDED(SHCreateStreamOnFileEx(filePath.c_str(), STGM_READ | STGM_SHARE_DENY_NONE,
                                                     FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &pStream)) && pStream) {
                    if (SUCCEEDED(pInitStream->Initialize(pStream, STGM_READ))) {
                        initialized = true;
                    }
                    pStream->Release();
                }
                pInitStream->Release();
            }

            // (2) Try IInitializeWithFile
            if (!initialized) {
                IInitializeWithFile* pInitFile = nullptr;
                if (SUCCEEDED(m_pPreviewHandler->QueryInterface(IID_PPV_ARGS(&pInitFile)))) {
                    if (SUCCEEDED(pInitFile->Initialize(filePath.c_str(), STGM_READ))) {
                        initialized = true;
                    }
                    pInitFile->Release();
                }
            }

            // (3) Try IInitializeWithItem
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
                m_pPreviewHandler->SetWindow(m_hwndContainer, &rcPreview);
                if (SUCCEEDED(m_pPreviewHandler->DoPreview())) {
                    return true;
                }
            }

            m_pPreviewHandler->Unload();
            m_pPreviewHandler->Release();
            m_pPreviewHandler = nullptr;
        }
    }

    // Mark PDF fallback if COM handler failed or absent
    if (lowerExt == L".pdf") {
        m_isPdf = true;
        return true;
    }

    // 3. Fallback: Text file preview (first 64KB)
    if (lowerExt == L".txt" || lowerExt == L".md" || lowerExt == L".json" || lowerExt == L".ini" ||
        lowerExt == L".cpp" || lowerExt == L".h" || lowerExt == L".c" || lowerExt == L".py" ||
        lowerExt == L".js" || lowerExt == L".log" || lowerExt == L".bat" || lowerExt == L".xml" ||
        lowerExt == L".html" || lowerExt == L".css" || lowerExt == L".cmd" || lowerExt == L".svg" ||
        lowerExt == L".yaml" || lowerExt == L".yml" || lowerExt == L".sql" || lowerExt == L".sh") {
        
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

    // 1. Image Preview
    if (m_hFallbackBitmap && m_imgWidth > 0 && m_imgHeight > 0) {
        int availW = rc.right - rc.left - 24;
        int availH = rc.bottom - rc.top - 48; // reserve space for text info at bottom
        if (availW > 10 && availH > 10) {
            // Keep aspect ratio
            double scaleW = (double)availW / m_imgWidth;
            double scaleH = (double)availH / m_imgHeight;
            double scale = std::min(std::min(scaleW, scaleH), 1.0); // Don't upscale small icons excessively

            int drawW = std::max(1, (int)(m_imgWidth * scale));
            int drawH = std::max(1, (int)(m_imgHeight * scale));

            int drawX = rc.left + 12 + (availW - drawW) / 2;
            int drawY = rc.top + 12 + (availH - drawH) / 2;

            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, m_hFallbackBitmap);

            int prevMode = SetStretchBltMode(hdc, HALFTONE);
            SetBrushOrgEx(hdc, 0, 0, nullptr);

            BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            if (!AlphaBlend(hdc, drawX, drawY, drawW, drawH, hdcMem, 0, 0, m_imgWidth, m_imgHeight, bf)) {
                StretchBlt(hdc, drawX, drawY, drawW, drawH, hdcMem, 0, 0, m_imgWidth, m_imgHeight, SRCCOPY);
            }

            SetStretchBltMode(hdc, prevMode);
            SelectObject(hdcMem, hOldBmp);
            DeleteDC(hdcMem);

            // Draw image information footer
            if (!m_imgInfoText.empty()) {
                RECT rcInfo = { rc.left + 8, rc.bottom - 32, rc.right - 8, rc.bottom - 8 };
                SetTextColor(hdc, Theme::Get().ClrTextSecondary());
                HFONT hFont = Theme::Get().GetUIFont(m_hwndContainer);
                HGDIOBJ oldF = SelectObject(hdc, hFont);
                DrawTextW(hdc, m_imgInfoText.c_str(), -1, &rcInfo, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, oldF);
            }
            return;
        }
    }

    // 2. PDF Fallback Card (when system has no PDF preview handler)
    if (m_isPdf) {
        HFONT hFontBold = Theme::Get().GetUIFontBold(m_hwndContainer);
        HGDIOBJ oldFont = SelectObject(hdc, hFontBold);
        SetTextColor(hdc, Theme::Get().ClrTextPrimary());

        std::wstring fileName = PathFindFileNameW(m_currentFile.c_str());
        RECT rcTitle = rc;
        rcTitle.top += (rc.bottom - rc.top) / 3;
        rcTitle.bottom = rcTitle.top + 30;
        DrawTextW(hdc, (L"📄  " + fileName).c_str(), -1, &rcTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SetTextColor(hdc, Theme::Get().ClrTextSecondary());
        HFONT hFont = Theme::Get().GetUIFont(m_hwndContainer);
        SelectObject(hdc, hFont);

        RECT rcHint = rcTitle;
        rcHint.top += 34;
        rcHint.bottom += 34;
        bool isZh = I18n::Get().IsChinese();
        std::wstring hint = isZh ? L"双击或按回车可在默认应用中打开 PDF" : L"Double-click or press Enter to open PDF in default app";
        DrawTextW(hdc, hint.c_str(), -1, &rcHint, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        return;
    }

    // 3. Text file preview
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
