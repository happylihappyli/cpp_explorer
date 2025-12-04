// 简化的C++资源管理器UI版本（Win32原生界面）
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <wchar.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// 控制台窗口相关函数
// 检测是否在控制台环境中运行
BOOL IsConsoleApp() {
    // 尝试获取控制台窗口句柄
    HWND consoleWindow = GetConsoleWindow();
    return consoleWindow != NULL;
}

// 分配控制台窗口（如果需要）
BOOL AllocateConsoleIfNeeded() {
    // 如果已经有控制台窗口，不需要重新分配
    if (IsConsoleApp()) {
        return TRUE;
    }
    
    // 分配新的控制台窗口
    if (!AllocConsole()) {
        return FALSE;
    }
    
    // 设置控制台标题
    SetConsoleTitleW(L"资源管理器 - 调试输出");
    
    // 使用简单的freopen重定向标准输出
    FILE* fp;
    
    // 重定向标准输出到控制台
    if (freopen_s(&fp, "CONOUT$", "w", stdout) != 0) {
        return FALSE;
    }
    
    // 重定向标准错误到控制台
    if (freopen_s(&fp, "CONOUT$", "w", stderr) != 0) {
        return FALSE;
    }
    
    // 重定向标准输入从控制台
    if (freopen_s(&fp, "CONIN$", "r", stdin) != 0) {
        return FALSE;
    }
    
    return TRUE;
}

// 日志输出函数
void LogMessage(const WCHAR* format, ...) {
    va_list args;
    va_start(args, format);
    
    WCHAR buffer[1024];
    vswprintf_s(buffer, 1024, format, args);
    
    // 输出到控制台（使用立即刷新的方式）
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != NULL && hConsole != INVALID_HANDLE_VALUE) {
        DWORD charsWritten;
        WCHAR consoleBuffer[2048];
        swprintf_s(consoleBuffer, 2048, L"[LOG] %s\n", buffer);
        WriteConsoleW(hConsole, consoleBuffer, wcslen(consoleBuffer), &charsWritten, NULL);
        FlushFileBuffers(hConsole);
    }
    
    // 同时输出到调试器
    OutputDebugStringW(L"[LOG] ");
    OutputDebugStringW(buffer);
    OutputDebugStringW(L"\n");
    
    va_end(args);
}

// 全局变量
HWND g_mainWindow = NULL;
HWND g_treeView = NULL;  // 左侧目录树
HWND g_listView = NULL;  // 右侧文件列表
HWND g_addressBar = NULL;
HWND g_goButton = NULL;
HWND g_backButton = NULL;
WCHAR g_currentPath[MAX_PATH] = {0};

// 获取当前工作目录
void getCurrentDirectory(WCHAR* buffer, DWORD bufferSize) {
    GetCurrentDirectoryW(bufferSize, buffer);
}

// 设置当前工作目录
void setCurrentDirectory(const WCHAR* path) {
    SetCurrentDirectoryW(path);
    lstrcpyW(g_currentPath, path);
    
    // 更新地址栏
    if (g_addressBar) {
        SetWindowTextW(g_addressBar, path);
    }
}

// 文件信息结构
typedef struct {
    WCHAR name[MAX_PATH];
    BOOL isDirectory;
    ULONGLONG size;
} FileInfo;

// 列出目录内容
int listDirectory(const WCHAR* path, FileInfo* files, int maxFiles) {
    WIN32_FIND_DATAW findData;
    WCHAR searchPath[MAX_PATH];
    int count = 0;
    
    // 构造搜索路径
    lstrcpyW(searchPath, path);
    lstrcatW(searchPath, L"\\*");
    
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    do {
        // 跳过.和..
        if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
            if (count < maxFiles) {
                lstrcpyW(files[count].name, findData.cFileName);
                files[count].isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                
                if (files[count].isDirectory) {
                    files[count].size = 0;
                } else {
                    ULARGE_INTEGER fileSize;
                    fileSize.LowPart = findData.nFileSizeLow;
                    fileSize.HighPart = findData.nFileSizeHigh;
                    files[count].size = fileSize.QuadPart;
                }
                
                count++;
            }
        }
    } while (FindNextFileW(hFind, &findData) && count < maxFiles);
    
    FindClose(hFind);
    return count;
}

// 格式化文件大小
void formatFileSize(ULONGLONG size, WCHAR* buffer, int bufferSize) {
    if (size == 0) {
        lstrcpyW(buffer, L"-");
        return;
    }
    
    const WCHAR* units[] = {L"bytes", L"KB", L"MB", L"GB", L"TB"};
    int unitIndex = 0;
    double fileSize = (double)size;
    
    while (fileSize >= 1024 && unitIndex < 4) {
        fileSize /= 1024;
        unitIndex++;
    }
    
    if (unitIndex == 0) {
        swprintf_s(buffer, bufferSize, L"%llu %s", size, units[unitIndex]);
    } else {
        swprintf_s(buffer, bufferSize, L"%.1f %s", fileSize, units[unitIndex]);
    }
}

// 更新ListView中的文件列表
void updateFileList() {
    LogMessage(L"[DEBUG] updateFileList 开始，当前路径: %s", g_currentPath);
    
    if (!g_listView) {
        LogMessage(L"[DEBUG] g_listView 为空，跳过更新");
        return;
    }
    
    // 清空现有项目
    LogMessage(L"[DEBUG] 清空ListView现有项目");
    SendMessageW(g_listView, LVM_DELETEALLITEMS, 0, 0);
    
    // 获取文件列表
    FileInfo files[1000];
    int fileCount = listDirectory(g_currentPath, files, 1000);
    LogMessage(L"[DEBUG] 获取到 %d 个文件/文件夹", fileCount);
    
    // 添加项目到ListView
    for (int i = 0; i < fileCount; ++i) {
        LVITEMW item = {0};
        item.iItem = i;
        item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
        item.pszText = files[i].name;
        item.lParam = i; // 存储索引以便后续查找
        item.iImage = files[i].isDirectory ? 0 : 1; // 0为文件夹图标，1为文件图标
                
        int index = SendMessageW(g_listView, LVM_INSERTITEMW, 0, (LPARAM)&item);
        
        if (index >= 0) {
            LogMessage(L"[DEBUG] 添加项目: %s (类型: %s)", files[i].name, files[i].isDirectory ? L"文件夹" : L"文件");
            
            // 设置大小列
            WCHAR sizeText[64];
            if (files[i].isDirectory) {
                lstrcpyW(sizeText, L"-");
            } else {
                formatFileSize(files[i].size, sizeText, 64);
            }
            
            LVITEMW subItem = {0};
            subItem.iItem = index;
            subItem.iSubItem = 1;
            subItem.mask = LVIF_TEXT;
            subItem.pszText = sizeText;
            SendMessageW(g_listView, LVM_SETITEMW, 0, (LPARAM)&subItem);
            
            // 设置类型列
            LPCWSTR typeText = files[i].isDirectory ? L"文件夹" : L"文件";
            subItem.iSubItem = 2;
            subItem.pszText = (LPWSTR)typeText;
            SendMessageW(g_listView, LVM_SETITEMW, 0, (LPARAM)&subItem);
        } else {
            LogMessage(L"[DEBUG] 添加项目失败: %s", files[i].name);
        }
    }
    
    LogMessage(L"[DEBUG] updateFileList 完成，共添加 %d 个项目", fileCount);
}

// 获取所有逻辑驱动器
void getLogicalDrives(WCHAR drives[][4], int* count) {
    LogMessage(L"getLogicalDrives 开始");
    
    *count = 0;
    DWORD driveMask = GetLogicalDrives();
    
    // 输出驱动器掩码
    LogMessage(L"驱动器掩码: 0x%X", driveMask);
    
    for (int i = 0; i < 26; i++) {
        if (driveMask & (1 << i)) {
            // 直接构造驱动器名称，避免缓冲区溢出
            drives[*count][0] = L'A' + i;
            drives[*count][1] = L':';
            drives[*count][2] = L'\\';
            drives[*count][3] = L'\0';
            
            // 输出找到的驱动器
            LogMessage(L"找到驱动器: %s", drives[*count]);
            
            (*count)++;
        }
    }
    
    // 输出总驱动器数量
    LogMessage(L"总共找到 %d 个驱动器", *count);
    
    LogMessage(L"getLogicalDrives 完成");
}

// 检查目录是否包含子目录
BOOL hasSubdirectories(const WCHAR* path) {
    WIN32_FIND_DATAW findData;
    WCHAR searchPath[MAX_PATH];
    
    lstrcpyW(searchPath, path);
    if (searchPath[lstrlenW(searchPath) - 1] != L'\\') {
        lstrcatW(searchPath, L"\\");
    }
    lstrcatW(searchPath, L"*");
    
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    BOOL hasSubdirs = FALSE;
    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
                hasSubdirs = TRUE;
                break;
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    return hasSubdirs;
}

// 获取节点的完整路径
void getNodeFullPath(HWND treeView, HTREEITEM hItem, WCHAR* fullPath, int bufferSize) {
    WCHAR pathParts[100][MAX_PATH];
    int level = 0;
    
    // 从当前节点向上遍历到根节点
    HTREEITEM currentItem = hItem;
    while (currentItem) {
        TVITEMW tvi = {0};
        tvi.hItem = currentItem;
        tvi.mask = TVIF_TEXT;
        tvi.pszText = pathParts[level];
        tvi.cchTextMax = MAX_PATH;
        
        if (!SendMessageW(treeView, TVM_GETITEMW, 0, (LPARAM)&tvi)) {
            break;
        }
        
        level++;
        currentItem = TreeView_GetParent(treeView, currentItem);
    }
    
    // 输出调试日志
    LogMessage(L"getNodeFullPath 节点层级数: %d", level);
    
    // 构造完整路径（从根到叶）
    fullPath[0] = L'\0';
    for (int i = level - 1; i >= 0; i--) {
        if (fullPath[0] != L'\0' && fullPath[lstrlenW(fullPath) - 1] != L'\\') {
            lstrcatW(fullPath, L"\\");
        }
        lstrcatW(fullPath, pathParts[i]);
        
        // 输出每一层路径
        LogMessage(L"路径层级 %d: %s", i, pathParts[i]);
    }
    
    // 如果是驱动器根目录，确保以反斜杠结尾
    if (lstrlenW(fullPath) == 2 && fullPath[1] == L':') {
        lstrcatW(fullPath, L"\\");
    }
    
    // 输出最终路径
    LogMessage(L"最终路径: %s", fullPath);
}

// 为目录节点添加占位符子节点（用于显示展开按钮）
void addPlaceholderNode(HWND treeView, HTREEITEM parent) {
    TVINSERTSTRUCTW tvis = {0};
    WCHAR placeholderText[] = L"加载中...";  // 使用可见文本作为占位符
    tvis.hParent = parent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvis.item.pszText = placeholderText;
    tvis.item.lParam = (LPARAM)1;  // 标记为占位符
    
    SendMessageW(treeView, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
}

// 展开目录节点
void expandDirectoryNode(HWND treeView, HTREEITEM hItem, const WCHAR* path) {
    // 输出调试日志
    LogMessage(L"expandDirectoryNode 路径: %s", path);
    
    // 删除所有现有子节点
    HTREEITEM hChild = TreeView_GetChild(treeView, hItem);
    while (hChild) {
        HTREEITEM hNext = TreeView_GetNextSibling(treeView, hChild);
        SendMessageW(treeView, TVM_DELETEITEM, 0, (LPARAM)hChild);
        hChild = hNext;
    }
    
    // 添加实际的子目录
    WIN32_FIND_DATAW findData;
    WCHAR searchPath[MAX_PATH];
    
    lstrcpyW(searchPath, path);
    if (searchPath[lstrlenW(searchPath) - 1] != L'\\') {
        lstrcatW(searchPath, L"\\");
    }
    lstrcatW(searchPath, L"*");
    
    // 输出搜索路径调试日志
    LogMessage(L"搜索路径: %s", searchPath);
    
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    LockWindowUpdate(treeView); // 锁定窗口更新
    if (hFind != INVALID_HANDLE_VALUE) {
        int dirCount = 0;
        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
                    dirCount++;
                    WCHAR subDirPath[MAX_PATH];
                    lstrcpyW(subDirPath, path);
                    if (subDirPath[lstrlenW(subDirPath) - 1] != L'\\') {
                        lstrcatW(subDirPath, L"\\");
                    }
                    lstrcatW(subDirPath, findData.cFileName);
                    
                    // 输出找到的目录名
                    LogMessage(L"找到目录: %s", findData.cFileName);
                    
                    TVINSERTSTRUCTW tvis = {0};
                    tvis.hParent = hItem;
                    tvis.hInsertAfter = TVI_LAST;
                    tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
                    tvis.item.pszText = findData.cFileName;
                    tvis.item.lParam = (LPARAM)0;  // 标记为实际目录
                    tvis.item.iImage = 0;  // 文件夹图标索引
                    tvis.item.iSelectedImage = 0;  // 选中时的文件夹图标索引
                    
                    HTREEITEM hSubItem = (HTREEITEM)SendMessageW(treeView, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
                    
                    if (hSubItem) {
                        LogMessage(L"目录节点添加成功: %s", findData.cFileName);
                        // 如果子目录还有子目录，添加占位符
                        if (hasSubdirectories(subDirPath)) {
                            addPlaceholderNode(treeView, hSubItem);
                        }
                    } else {
                LogMessage(L"目录节点添加失败: %s", findData.cFileName);
            }

                }
            }
        } while (FindNextFileW(hFind, &findData));
        
        // 输出找到的目录数量
        LogMessage(L"[DEBUG] 总共找到 %d 个目录\n", dirCount);
        
        InvalidateRect(treeView, NULL, TRUE);
        UpdateWindow(treeView);
        
        FindClose(hFind);
        LockWindowUpdate(NULL); // 解锁窗口更新并强制重绘
    } else {
        // 输出详细的错误信息
        DWORD error = GetLastError();
        LogMessage(L"[DEBUG] FindFirstFileW 失败，错误代码: %d\n", error);
        
        // 添加一个错误提示节点
        TVINSERTSTRUCTW tvis = {0};
        tvis.hParent = hItem;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = L"[无法访问]";
        tvis.item.lParam = (LPARAM)2;  // 标记为错误节点
        
        SendMessageW(treeView, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
    }
}

// 更新目录树
void updateDirectoryTree() {
    LogMessage(L"[DEBUG] updateDirectoryTree 开始\n");
    
    if (!g_treeView) {
        LogMessage(L"[DEBUG] g_treeView 为空\n");
        return;
    }
    
    // 清空现有项目
    LogMessage(L"[DEBUG] 清空现有项目\n");
    SendMessageW(g_treeView, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
    
    // 获取所有逻辑驱动器
    WCHAR drives[26][4];
    int driveCount = 0;
    getLogicalDrives(drives, &driveCount);
    
    // 输出驱动器数量
    LogMessage(L"[DEBUG] 检测到 %d 个驱动器\n", driveCount);
    
    // 为每个驱动器添加节点
    for (int i = 0; i < driveCount; i++) {
        // 输出驱动器名称
        LogMessage(L"[DEBUG] 处理驱动器: %s\n", drives[i]);
        
        // 直接使用驱动器名称，确保文本正确
        WCHAR driveName[4] = {0};
        lstrcpyW(driveName, drives[i]);  // 驱动器名称已经是"X:\\"格式
        
        // 添加驱动器节点
        TVINSERTSTRUCTW tvis = {0};
        tvis.hParent = NULL;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
        tvis.item.pszText = driveName;
        tvis.item.lParam = (LPARAM)0;  // 标记为实际目录
        tvis.item.iImage = 1;  // 驱动器图标索引
        tvis.item.iSelectedImage = 1;  // 选中时的驱动器图标索引
        
        HTREEITEM hDriveItem = (HTREEITEM)SendMessageW(g_treeView, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
        
        if (hDriveItem) {
            LogMessage(L"[DEBUG] 驱动器节点添加成功: %s\n", driveName);
            // 为驱动器添加占位符子节点（用于显示展开按钮）
            addPlaceholderNode(g_treeView, hDriveItem);
        } else {
            LogMessage(L"[DEBUG] 驱动器节点添加失败: %s\n", driveName);
        }
    }
    
    LogMessage(L"[DEBUG] updateDirectoryTree 完成\n");
}

// 处理目录树节点展开
void handleTreeItemExpanding(HWND treeView, HTREEITEM hItem) {
    // 获取节点的完整路径
    WCHAR fullPath[MAX_PATH] = {0};
    getNodeFullPath(treeView, hItem, fullPath, MAX_PATH);
    
    // 输出调试日志
    LogMessage(L"[DEBUG] 展开节点: %s\n", fullPath);
    
    // 获取节点信息
    TVITEMW tvi = {0};
    WCHAR itemText[MAX_PATH] = {0};
    tvi.hItem = hItem;
    tvi.mask = TVIF_TEXT | TVIF_PARAM;
    tvi.pszText = itemText;
    tvi.cchTextMax = MAX_PATH;
    
    if (SendMessageW(treeView, TVM_GETITEMW, 0, (LPARAM)&tvi)) {
        // 如果是占位符节点，不需要展开
        if (tvi.lParam == 1) {
            LogMessage(L"[DEBUG] 占位符节点，跳过展开\n");
            return;
        }
        
        // 如果是错误节点，不需要展开
        if (tvi.lParam == 2) {
            LogMessage(L"[DEBUG] 错误节点，跳过展开\n");
            return;
        }
        
        // 展开目录节点
        LogMessage(L"[DEBUG] 开始展开目录节点\n");
        expandDirectoryNode(treeView, hItem, fullPath);
    HTREEITEM hSelected = TreeView_GetSelection(treeView);
    if (hSelected == hItem) {
        setCurrentDirectory(fullPath);
        updateFileList();
    }
    }
}

// 窗口过程函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // 初始化通用控件
            INITCOMMONCONTROLSEX icex;
            icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
            icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES;
            InitCommonControlsEx(&icex);
            
            // 设置系统字体以支持中文
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            if (hFont) {
                SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
            }
            
            // 创建地址栏
            g_addressBar = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                250, 10, 450, 25,
                hwnd, NULL, NULL, NULL
            );
            
            // 创建后退按钮
            g_backButton = CreateWindowExW(
                0, L"BUTTON", L"← 后退",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                170, 10, 70, 25,
                hwnd, NULL, NULL, NULL
            );
            
            // 创建前往按钮
            g_goButton = CreateWindowExW(
                0, L"BUTTON", L"前往",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                710, 10, 80, 25,
                hwnd, NULL, NULL, NULL
            );
            
            // 创建TreeView (左侧目录树)
            g_treeView = CreateWindowExW(
                0, WC_TREEVIEWW, L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
                10, 50, 200, 500,
                hwnd, NULL, NULL, NULL
            );
            
            // 创建ListView (右侧文件列表)
            g_listView = CreateWindowExW(
                0, WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                220, 50, 570, 500,
                hwnd, NULL, NULL, NULL
            );
            
            // 创建图像列表用于TreeView
            HIMAGELIST hTreeImageList = ImageList_Create(16, 16, ILC_COLOR32, 2, 10);
            HICON hFolderIcon = LoadIcon(NULL, IDI_APPLICATION);  // 文件夹图标
            HICON hDriveIcon = LoadIcon(NULL, IDI_APPLICATION);  // 驱动器图标
            ImageList_AddIcon(hTreeImageList, hFolderIcon);
            ImageList_AddIcon(hTreeImageList, hDriveIcon);
            SendMessageW(g_treeView, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)hTreeImageList);
            
            // 创建图像列表用于ListView
            HIMAGELIST hListImageList = ImageList_Create(16, 16, ILC_COLOR32, 2, 10);
            HICON hFileIcon = LoadIcon(NULL, IDI_APPLICATION);  // 文件图标
            ImageList_AddIcon(hListImageList, hFolderIcon);
            ImageList_AddIcon(hListImageList, hFileIcon);
            SendMessageW(g_listView, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)hListImageList);
            
            // 设置ListView样式
            DWORD exStyles = SendMessageW(g_listView, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
            exStyles |= LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES;
            SendMessageW(g_listView, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, exStyles);
            
            // 添加列
            LVCOLUMNW col1 = {0};
            col1.mask = LVCF_TEXT | LVCF_WIDTH;
            col1.cx = 200;
            col1.pszText = (LPWSTR)L"名称";
            SendMessageW(g_listView, LVM_INSERTCOLUMNW, 0, (LPARAM)&col1);
            
            LVCOLUMNW col2 = {0};
            col2.mask = LVCF_TEXT | LVCF_WIDTH;
            col2.cx = 150;
            col2.pszText = (LPWSTR)L"大小";
            SendMessageW(g_listView, LVM_INSERTCOLUMNW, 1, (LPARAM)&col2);
            
            LVCOLUMNW col3 = {0};
            col3.mask = LVCF_TEXT | LVCF_WIDTH;
            col3.cx = 100;
            col3.pszText = (LPWSTR)L"类型";
            SendMessageW(g_listView, LVM_INSERTCOLUMNW, 2, (LPARAM)&col3);
            
            break;
        }
        
        case WM_SIZE: {
            // 调整控件大小
            if (g_backButton) {
                MoveWindow(g_backButton, 170, 10, 70, 25, TRUE);
            }
            
            if (g_addressBar) {
                MoveWindow(g_addressBar, 250, 10, LOWORD(lParam) - 340, 25, TRUE);
            }
            
            if (g_goButton) {
                MoveWindow(g_goButton, LOWORD(lParam) - 90, 10, 80, 25, TRUE);
            }
            
            // 调整TreeView大小 (左侧目录树)
            if (g_treeView) {
                MoveWindow(g_treeView, 10, 50, 200, HIWORD(lParam) - 60, TRUE);
            }
            
            // 调整ListView大小 (右侧文件列表)
            if (g_listView) {
                MoveWindow(g_listView, 220, 50, LOWORD(lParam) - 230, HIWORD(lParam) - 60, TRUE);
            }
            break;
        }
        
        case WM_COMMAND: {
            if ((HWND)lParam == g_goButton) {
                if (HIWORD(wParam) == BN_CLICKED) {
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
            } else if ((HWND)lParam == g_backButton && HIWORD(wParam) == BN_CLICKED) {
                // 返回上级目录
                WCHAR* lastSlash = wcsrchr(g_currentPath, L'\\');
                if (lastSlash) {
                    *lastSlash = L'\0';
                    if (lstrlenW(g_currentPath) == 2 && g_currentPath[1] == L':') {
                        // 如果是驱动器根目录，添加反斜杠
                        lstrcatW(g_currentPath, L"\\");
                    }
                    setCurrentDirectory(g_currentPath);
                    updateFileList();
                }
            }
            break;
        }
        
        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            
            // 处理ListView的通知消息
            if (nmhdr->hwndFrom == g_listView) {
                switch (nmhdr->code) {
                    case NM_DBLCLK: {
                        LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
                        if (lpnmitem->iItem != -1) {
                            // 获取选中项的文本
                            WCHAR itemName[MAX_PATH] = {0};
                            LVITEMW item = {0};
                            item.iItem = lpnmitem->iItem;
                            item.iSubItem = 0;
                            item.mask = LVIF_TEXT;
                            item.pszText = itemName;
                            item.cchTextMax = MAX_PATH;
                            SendMessageW(g_listView, LVM_GETITEMW, 0, (LPARAM)&item);
                            
                            // 获取该项的类型（从第三列）
                            WCHAR itemType[32] = {0};
                            item.iSubItem = 2;
                            item.pszText = itemType;
                            item.cchTextMax = 32;
                            SendMessageW(g_listView, LVM_GETITEMW, 0, (LPARAM)&item);
                            
                            if (wcscmp(itemType, L"文件夹") == 0) {
                                // 导航到子目录
                                WCHAR newPath[MAX_PATH] = {0};
                                lstrcpyW(newPath, g_currentPath);
                                if (newPath[lstrlenW(newPath) - 1] != L'\\') {
                                    lstrcatW(newPath, L"\\");
                                }
                                lstrcatW(newPath, itemName);
                                
                                // 检查路径是否存在且为目录
                                DWORD attributes = GetFileAttributesW(newPath);
                                if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                                    setCurrentDirectory(newPath);
                                    updateFileList();
                                    // 更新目录树
                                    updateDirectoryTree();
                                } else {
                                    MessageBoxW(hwnd, L"无效的目录路径", L"错误", MB_OK | MB_ICONERROR);
                                }
                            } else {
                                // 打开文件
                                WCHAR filePath[MAX_PATH] = {0};
                                lstrcpyW(filePath, g_currentPath);
                                if (filePath[lstrlenW(filePath) - 1] != L'\\') {
                                    lstrcatW(filePath, L"\\");
                                }
                                lstrcatW(filePath, itemName);
                                ShellExecuteW(hwnd, L"open", filePath, NULL, NULL, SW_SHOWNORMAL);
                            }
                        }
                        break;
                    }
                }
            }
            
            // 处理TreeView的通知消息
            else if (nmhdr->hwndFrom == g_treeView) {
                switch (nmhdr->code) {
                    case NM_DBLCLK: {
                        HTREEITEM hSelectedItem = TreeView_GetSelection(g_treeView);
                        if (hSelectedItem) {
                            // 展开节点
                            handleTreeItemExpanding(g_treeView, hSelectedItem);
                            // 更新ListView（如果需要，虽然SELCHANGED会处理）
                            WCHAR fullPath[MAX_PATH] = {0};
                            getNodeFullPath(g_treeView, hSelectedItem, fullPath, MAX_PATH);
                            setCurrentDirectory(fullPath);
                            updateFileList();
                        }
                        break;
                    }
                    
                    case TVN_ITEMEXPANDINGW: {
                        LPNMTREEVIEWW pnmtv = (LPNMTREEVIEWW)lParam;
                        if (pnmtv->action == TVE_EXPAND) {
                            LogMessage(L"[DEBUG] TVN_ITEMEXPANDINGW: 展开节点");
                            handleTreeItemExpanding(g_treeView, pnmtv->itemNew.hItem);
                        } else if (pnmtv->action == TVE_COLLAPSE) {
                            LogMessage(L"[DEBUG] TVN_ITEMEXPANDINGW: 折叠节点");
                        }
                        return 0;  // 允许展开/折叠操作
                    }
                    case TVN_SELCHANGEDW: {
                        LPNMTREEVIEWW pnmtv = (LPNMTREEVIEWW)lParam;
                        HTREEITEM hItem = pnmtv->itemNew.hItem;
                        if (hItem) {
                            WCHAR fullPath[MAX_PATH] = {0};
                            getNodeFullPath(g_treeView, hItem, fullPath, MAX_PATH);
                            LogMessage(L"[DEBUG] 节点选中改变: %s", fullPath);
                            setCurrentDirectory(fullPath);
                            updateFileList();
                        }
                        break;
                    }
                }
            }
            
            break;
        }
        
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// 注册窗口类
BOOL RegisterWindowClass(HINSTANCE hInstance) {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ExplorerWindowClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    return RegisterClassW(&wc) != 0;
}

// 创建主窗口
HWND CreateMainWindow(HINSTANCE hInstance) {
    HWND hwnd = CreateWindowExW(
        0,
        L"ExplorerWindowClass",
        L"C++ 资源管理器",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
        NULL, NULL, hInstance, NULL
    );
    
    if (hwnd) {
        // 验证窗口标题
        WCHAR windowTitle[256];
        GetWindowTextW(hwnd, windowTitle, 256);
        LogMessage(L"窗口标题: %s", windowTitle);
        
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        
        LogMessage(L"主窗口显示完成");
    }
    
    return hwnd;
}

// WinMain入口点
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 分配控制台窗口并输出日志
    LogMessage(L"资源管理器程序启动中...");
    
    if (!AllocateConsoleIfNeeded()) {
        MessageBoxW(NULL, L"无法分配控制台窗口", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    LogMessage(L"控制台窗口已成功分配");
    
    // 初始化COM
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    LogMessage(L"COM初始化完成");
    
    // 注册窗口类
    if (!RegisterWindowClass(hInstance)) {
        MessageBoxW(NULL, L"窗口类注册失败", L"错误", MB_OK | MB_ICONERROR);
        LogMessage(L"窗口类注册失败");
        return 1;
    }
    LogMessage(L"窗口类注册成功");
    
    // 获取当前目录
    getCurrentDirectory(g_currentPath, MAX_PATH);
    LogMessage(L"当前目录: %s", g_currentPath);
    
    // 创建主窗口
    g_mainWindow = CreateMainWindow(hInstance);
    if (!g_mainWindow) {
        MessageBoxW(NULL, L"主窗口创建失败", L"错误", MB_OK | MB_ICONERROR);
        LogMessage(L"主窗口创建失败");
        return 1;
    }
    LogMessage(L"主窗口创建成功");
    
    // 设置地址栏初始值
    if (g_addressBar) {
        SetWindowTextW(g_addressBar, g_currentPath);
        LogMessage(L"地址栏设置完成: %s", g_currentPath);
    }
    
    // 加载目录树
    LogMessage(L"开始加载目录树...");
    updateDirectoryTree();
    LogMessage(L"目录树加载完成");
    
    // 加载初始文件列表
    LogMessage(L"开始加载文件列表...");
    updateFileList();
    LogMessage(L"文件列表加载完成");
    
    LogMessage(L"资源管理器程序启动完成，进入消息循环");
    
    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    LogMessage(L"资源管理器程序退出");
    
    // 清理COM
    CoUninitialize();
    
    return 0;
}