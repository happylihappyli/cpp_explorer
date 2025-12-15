#ifndef NOTIFICATION_HANDLERS_H
#define NOTIFICATION_HANDLERS_H

#include <windows.h>
#include <string>

// 前向声明
LRESULT HandleNotificationMessages(HWND hwnd, WPARAM wParam, LPARAM lParam);

struct ItemSortData {
    std::wstring name;
    BOOL isDir;
    ULONGLONG sizeNumeric;
    FILETIME created;
    FILETIME modified;
    BOOL isPartial;
};

#endif // NOTIFICATION_HANDLERS_H
