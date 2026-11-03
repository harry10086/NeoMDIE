#pragma once
#include "..\Common.h"

class PreviewHandlerHost {
public:
    PreviewHandlerHost();
    ~PreviewHandlerHost();

    bool Attach(HWND hwndContainer);
    void Detach();

    bool LoadFile(const std::wstring& filePath);
    void Unload();
    void Resize(const RECT& rc);
    void DrawFallback(HDC hdc, const RECT& rc);

    bool HasActiveHandler() const { return m_pPreviewHandler != nullptr; }
    std::wstring GetCurrentFile() const { return m_currentFile; }

private:
    bool QueryPreviewHandlerClsid(const std::wstring& ext, CLSID& outClsid);

    HWND m_hwndContainer = nullptr;
    IPreviewHandler* m_pPreviewHandler = nullptr;
    std::wstring m_currentFile;
    std::wstring m_fallbackText;
    HBITMAP m_hFallbackBitmap = nullptr;
};
