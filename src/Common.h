#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <shellapi.h>
#include <wininet.h>
#include <strsafe.h>

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wininet.lib")

template <typename T>
inline void SafeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

struct PaneViewState {
    int topIndex = 0;
    std::wstring topItemPath;
    std::wstring focusedPath;
    std::vector<std::wstring> selectedPaths;
    int sortCol = 0;
    bool sortAsc = true;
    int viewMode = -1;
};

namespace Util {
    inline std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
        return result;
    }

    inline std::string WideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size, nullptr, nullptr);
        return result;
    }

    inline std::wstring AnsiToWide(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), nullptr, 0);
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), &result[0], size);
        return result;
    }

    inline std::string WideToAnsi(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), &result[0], size, nullptr, nullptr);
        return result;
    }

    inline std::wstring DecodeOutput(const std::string& str) {
        if (str.empty()) return std::wstring();
        int utf8Size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), nullptr, 0);
        if (utf8Size > 0) {
            std::wstring result(utf8Size, 0);
            MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], utf8Size);
            return result;
        }
        int ansiSize = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), nullptr, 0);
        if (ansiSize > 0) {
            std::wstring result(ansiSize, 0);
            MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), &result[0], ansiSize);
            return result;
        }
        int oemSize = MultiByteToWideChar(CP_OEMCP, 0, str.c_str(), (int)str.size(), nullptr, 0);
        if (oemSize > 0) {
            std::wstring result(oemSize, 0);
            MultiByteToWideChar(CP_OEMCP, 0, str.c_str(), (int)str.size(), &result[0], oemSize);
            return result;
        }
        return std::wstring();
    }

    inline std::wstring FormatBytes(uint64_t bytes) {
        const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
        int idx = 0;
        double size = (double)bytes;
        while (size >= 1024.0 && idx < 4) {
            size /= 1024.0;
            idx++;
        }
        wchar_t buf[64];
        if (idx == 0) {
            StringCchPrintfW(buf, 64, L"%u %s", (unsigned int)bytes, units[idx]);
        } else {
            StringCchPrintfW(buf, 64, L"%.1f %s", size, units[idx]);
        }
        return buf;
    }

    inline std::wstring FormatFileTime(const FILETIME& ft) {
        if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0) {
            return L"";
        }
        SYSTEMTIME stUTC, stLocal;
        if (!FileTimeToSystemTime(&ft, &stUTC)) {
            return L"";
        }
        SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);
        wchar_t buf[64];
        StringCchPrintfW(buf, 64, L"%04d-%02d-%02d %02d:%02d",
            stLocal.wYear, stLocal.wMonth, stLocal.wDay,
            stLocal.wHour, stLocal.wMinute);
        return buf;
    }

    inline std::wstring FormatAttributes(DWORD attr) {
        std::wstring s;
        if (attr & FILE_ATTRIBUTE_READONLY) s += L"R";
        if (attr & FILE_ATTRIBUTE_HIDDEN) s += L"H";
        if (attr & FILE_ATTRIBUTE_SYSTEM) s += L"S";
        if (attr & FILE_ATTRIBUTE_ARCHIVE) s += L"A";
        if (attr & FILE_ATTRIBUTE_COMPRESSED) s += L"C";
        if (attr & FILE_ATTRIBUTE_ENCRYPTED) s += L"E";
        return s;
    }

    inline std::wstring NormalizePath(const std::wstring& path) {
        if (path.empty()) return path;
        if (path.find(L"20D04FE0-3AEA-1069-A2D8-08002B30309D") != std::wstring::npos) {
            return L"This PC";
        }
        std::wstring p = path;
        std::replace(p.begin(), p.end(), L'/', L'\\');
        while (p.length() > 3 && p.back() == L'\\') {
            p.pop_back();
        }
        return p;
    }

    inline std::wstring CombinePath(const std::wstring& dir, const std::wstring& file) {
        if (dir.empty()) return file;
        if (file.empty()) return dir;
        std::wstring d = dir;
        std::replace(d.begin(), d.end(), L'/', L'\\');
        if (d.back() == L'\\') {
            return d + file;
        }
        return d + L"\\" + file;
    }

    inline std::wstring GetExecutableDir() {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        PathRemoveFileSpecW(buf);
        return buf;
    }
}
