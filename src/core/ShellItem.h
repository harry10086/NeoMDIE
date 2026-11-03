#pragma once
#include "..\Common.h"

struct NeoShellItem {
    std::wstring name;
    std::wstring fullPath;
    std::wstring ext;
    std::wstring typeName;
    uint64_t     size = 0;
    FILETIME     modifiedTime = { 0 };
    FILETIME     createdTime = { 0 };
    DWORD        attributes = 0;
    std::wstring authors;
    std::wstring title;
    std::wstring tags;
    int          iconIndex = 0;
    bool         isFolder = false;
    bool         isDrive = false;
    bool         isSizeCalculated = false;

    bool MatchesFilter(const std::wstring& query) const;
};

enum class SortColumn {
    Name = 0,
    DateModified = 1,
    Type = 2,
    Size = 3,
    DateCreated = 4,
    Attributes = 5,
    Authors = 6,
    Title = 7
};

enum class ColumnId {
    Name = 0,
    DateModified = 1,
    Type = 2,
    Size = 3,
    DateCreated = 4,
    Attributes = 5,
    Authors = 6,
    Title = 7,
    Tags = 8,
    Path = 9,
    Count = 10
};

bool CompareShellItems(const NeoShellItem& a, const NeoShellItem& b, SortColumn col, bool ascending);
