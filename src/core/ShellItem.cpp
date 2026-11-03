#include "ShellItem.h"

bool NeoShellItem::MatchesFilter(const std::wstring& query) const {
    if (query.empty()) return true;

    // Wildcard or case-insensitive substring
    std::wstring lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::towlower);

    std::wstring lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);

    // If query starts with *., match extension
    if (lowerQuery.length() > 2 && lowerQuery[0] == L'*' && lowerQuery[1] == L'.') {
        std::wstring targetExt = lowerQuery.substr(1);
        std::wstring lowerExt = ext;
        std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::towlower);
        return lowerExt == targetExt;
    }

    // Direct substring match
    return lowerName.find(lowerQuery) != std::wstring::npos;
}

bool CompareShellItems(const NeoShellItem& a, const NeoShellItem& b, SortColumn col, bool ascending) {
    // Folders always come first
    if (a.isFolder != b.isFolder) {
        return a.isFolder;
    }

    int result = 0;
    switch (col) {
    case SortColumn::Name:
        result = CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE,
                                 a.name.c_str(), -1, b.name.c_str(), -1, nullptr, nullptr, 0) - CSTR_EQUAL;
        break;
    case SortColumn::DateModified: {
        ULARGE_INTEGER ta, tb;
        ta.LowPart = a.modifiedTime.dwLowDateTime;
        ta.HighPart = a.modifiedTime.dwHighDateTime;
        tb.LowPart = b.modifiedTime.dwLowDateTime;
        tb.HighPart = b.modifiedTime.dwHighDateTime;
        if (ta.QuadPart < tb.QuadPart) result = -1;
        else if (ta.QuadPart > tb.QuadPart) result = 1;
        else result = 0;
        break;
    }
    case SortColumn::Type:
        result = CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE,
                                 a.typeName.c_str(), -1, b.typeName.c_str(), -1, nullptr, nullptr, 0) - CSTR_EQUAL;
        if (result == 0) {
            result = CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE,
                                     a.name.c_str(), -1, b.name.c_str(), -1, nullptr, nullptr, 0) - CSTR_EQUAL;
        }
        break;
    case SortColumn::Size:
        if (a.size < b.size) result = -1;
        else if (a.size > b.size) result = 1;
        else result = 0;
        break;
    case SortColumn::DateCreated: {
        ULARGE_INTEGER ta, tb;
        ta.LowPart = a.createdTime.dwLowDateTime;
        ta.HighPart = a.createdTime.dwHighDateTime;
        tb.LowPart = b.createdTime.dwLowDateTime;
        tb.HighPart = b.createdTime.dwHighDateTime;
        if (ta.QuadPart < tb.QuadPart) result = -1;
        else if (ta.QuadPart > tb.QuadPart) result = 1;
        else result = 0;
        break;
    }
    case SortColumn::Attributes:
        if (a.attributes < b.attributes) result = -1;
        else if (a.attributes > b.attributes) result = 1;
        else result = 0;
        break;
    case SortColumn::Authors:
        result = CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE,
                                 a.authors.c_str(), -1, b.authors.c_str(), -1, nullptr, nullptr, 0) - CSTR_EQUAL;
        break;
    case SortColumn::Title:
        result = CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE,
                                 a.title.c_str(), -1, b.title.c_str(), -1, nullptr, nullptr, 0) - CSTR_EQUAL;
        break;
    }

    return ascending ? (result < 0) : (result > 0);
}
