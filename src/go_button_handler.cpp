#include "go_button_handler.h"
#include "favorites.h"
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>

// 声明全局变量（这些应该在其他地方定义）
extern HWND g_addressBar;
extern WCHAR g_currentPath[MAX_PATH];

// 声明外部函数
void setCurrentDirectory(const WCHAR* path);
void updateFileList();

// 实现"前往"按钮点击事件处理函数
void HandleGoButtonClick(HWND hwnd) {
    // 获取地址栏内容
    WCHAR path[MAX_PATH] = {0};
    GetWindowTextW(g_addressBar, path, MAX_PATH);
    
    if (lstrlenW(path) > 0) {
        // 检查路径是否存在且为目录
        DWORD attributes = GetFileAttributesW(path);
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            setCurrentDirectory(path);
            updateFileList();
        } else {
            MessageBoxW(hwnd, L"无效的目录路径", L"错误", MB_OK | MB_ICONERROR);
        }
    }
}