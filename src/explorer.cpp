// 简化的C++资源管理器UI版本（Win32原生界面）
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <wchar.h>
#include <stdio.h>
#include "favorites.h"
#include "notification_handlers.h"
#include "file_utils.h"
#include "tree_utils.h"
#include "log.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#define IDM_DEBUG 1001

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



// 全局变量
HWND g_mainWindow = NULL;
HWND g_treeView = NULL;  // 左侧目录树
HWND g_listView = NULL;  // 右侧文件列表
HWND g_addressBar = NULL;
HWND g_goButton = NULL;
HWND g_backButton = NULL;
HWND g_addFavoriteButton = NULL;  // 添加收藏按钮
// 移除了单独的收藏夹面板，将其集成到目录树中
WCHAR g_currentPath[MAX_PATH] = {0};
HTREEITEM g_favoritesNode = NULL;  // 收藏夹节点

// 地址栏原始窗口过程
WNDPROC g_OriginalAddressBarProc = NULL;

// 分隔条相关变量
int g_splitterPos = 215; // 分隔条位置
BOOL g_isDraggingSplitter = FALSE;
#define SPLITTER_WIDTH 5 // 分隔条宽度

// 自定义提示窗口相关变量
HWND g_tooltipWindow = NULL;
UINT_PTR g_tooltipTimer = 0;

// 函数声明
void HandleGoButtonClick(HWND hwnd);
void HandleBackButtonClick();
void HandleFavoriteCommands(WPARAM wParam);
void HandleListViewDoubleClick(HWND hwnd, LPARAM lParam);
void HandleTreeViewDoubleClick(HWND hwnd, HWND mainWindow);
void HandleDebugCommand(HWND hwnd, WPARAM wParam);
BOOL RegisterWindowClass(HINSTANCE hInstance);
HWND CreateMainWindow(HINSTANCE hInstance);

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

// 自定义提示窗口类名
#define TOOLTIP_WINDOW_CLASS L"CustomTooltipWindow"

// 自定义提示窗口函数声明
LRESULT CALLBACK TooltipWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ShowCustomTooltip(HWND parent, const WCHAR* text);
void HideCustomTooltip();

// 添加当前路径到收藏夹


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
    
    // 检查当前路径是否为收藏夹路径的特殊情况
    BOOL isFavoritesPath = FALSE;
    if (wcsncmp(g_currentPath, L"★ 收藏夹", 5) == 0) {
        isFavoritesPath = TRUE;
        LogMessage(L"[DEBUG] 当前为收藏夹路径");
    }
    
    if (isFavoritesPath) {
        // 显示收藏夹项作为文件列表
        for (int i = 0; i < g_favoriteCount; ++i) {
            LVITEMW item = {0};
            item.iItem = i;
            item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
            item.pszText = g_favorites[i].name;
            item.lParam = i; // 存储索引以便后续查找
            item.iImage = 0; // 文件夹图标
                
            int index = SendMessageW(g_listView, LVM_INSERTITEMW, 0, (LPARAM)&item);
            
            if (index >= 0) {
                LogMessage(L"[DEBUG] 添加收藏夹项目: %s", g_favorites[i].name);
                
                // 设置大小列
                WCHAR sizeText[64];
                lstrcpyW(sizeText, L"-");
                
                LVITEMW subItem = {0};
                subItem.iItem = index;
                subItem.iSubItem = 1;
                subItem.mask = LVIF_TEXT;
                subItem.pszText = sizeText;
                SendMessageW(g_listView, LVM_SETITEMW, 0, (LPARAM)&subItem);
                
                // 设置类型列
                LPCWSTR typeText = L"收藏夹项";
                subItem.iSubItem = 2;
                subItem.pszText = (LPWSTR)typeText;
                SendMessageW(g_listView, LVM_SETITEMW, 0, (LPARAM)&subItem);
            } else {
                LogMessage(L"[DEBUG] 添加收藏夹项目失败: %s", g_favorites[i].name);
            }
        }
        
        LogMessage(L"[DEBUG] updateFileList 完成，共添加 %d 个收藏夹项目", g_favoriteCount);
    } else {
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
    
    // 简化代码，移除重复的图标获取逻辑
}

// 创建收藏夹节点






// 地址栏子类化过程，用于处理回车键
LRESULT CALLBACK AddressBarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
        // 当按下回车键时，调用HandleGoButtonClick
        HandleGoButtonClick(GetParent(hwnd));
        return 0;
    }
    return CallWindowProc(g_OriginalAddressBarProc, hwnd, uMsg, wParam, lParam);
}

// 处理WM_CREATE消息的函数
void HandleCreateMessage(HWND hwnd) {
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
    
    // 子类化地址栏以处理回车键
    if (g_addressBar) {
        g_OriginalAddressBarProc = (WNDPROC)SetWindowLongPtr(g_addressBar, GWLP_WNDPROC, (LONG_PTR)AddressBarProc);
    }
    
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
    
    // 不再创建添加收藏按钮，改用右键菜单方式
    
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
    
    // 获取系统图标 - 使用SHGFI_USEFILEATTRIBUTES获取标准图标
    SHFILEINFOW sfi = {0};
    
    // 获取文件夹图标
    SHGetFileInfoW(L"Folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi), 
        SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON);
    HICON hFolderIcon = sfi.hIcon;

    // 获取文件图标 - 使用.txt扩展名获取关联图标
    SHGetFileInfoW(L".txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), 
        SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON);
    HICON hFileIcon = sfi.hIcon;
    
    // 获取驱动器图标
    SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), 
        SHGFI_ICON | SHGFI_SMALLICON);
    HICON hDriveIcon = sfi.hIcon;

    // 创建图像列表用于TreeView - 使用系统主题适配的图标尺寸
    HIMAGELIST hTreeImageList = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 
        ILC_COLOR32 | ILC_MASK, 2, 10);
    // 设置图像列表的背景色为白色
    ImageList_SetBkColor(hTreeImageList, RGB(255, 255, 255));
    // 添加图标
    ImageList_AddIcon(hTreeImageList, hFolderIcon);
    ImageList_AddIcon(hTreeImageList, hDriveIcon);
    SendMessageW(g_treeView, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)hTreeImageList);
    
    // 创建图像列表用于ListView
    HIMAGELIST hListImageList = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 
        ILC_COLOR32 | ILC_MASK, 2, 10);
    ImageList_SetBkColor(hListImageList, RGB(255, 255, 255));
    ImageList_AddIcon(hListImageList, hFolderIcon);
    ImageList_AddIcon(hListImageList, hFileIcon);
    SendMessageW(g_listView, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)hListImageList);

    // 清理图标句柄
    DestroyIcon(hFolderIcon);
    DestroyIcon(hFileIcon);
    DestroyIcon(hDriveIcon);
    
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
}

// 处理WM_SIZE消息的函数
void HandleSizeMessage(HWND hwnd, WPARAM wParam, LPARAM lParam) {
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
    
    // 不再调整收藏夹按钮大小，已移除该按钮
    
    int clientWidth = LOWORD(lParam);
    int clientHeight = HIWORD(lParam);
    
    // 确保分隔条位置在合理范围内
    if (g_splitterPos < 100) g_splitterPos = 100;
    if (g_splitterPos > clientWidth - 100) g_splitterPos = clientWidth - 100;
    
    // 调整TreeView大小 (左侧目录树)
    if (g_treeView) {
        // TreeView从x=10开始，宽度为 g_splitterPos - 10
        MoveWindow(g_treeView, 10, 50, g_splitterPos - 10, clientHeight - 60, TRUE);
    }
    
    // 调整ListView大小 (右侧文件列表)
    if (g_listView) {
        // ListView从 g_splitterPos + SPLITTER_WIDTH 开始
        int listViewX = g_splitterPos + SPLITTER_WIDTH;
        int listViewWidth = clientWidth - listViewX - 10; // 右侧保留10像素边距
        if (listViewWidth < 0) listViewWidth = 0;
        
        MoveWindow(g_listView, listViewX, 50, listViewWidth, clientHeight - 60, TRUE);
    }
}

// 窗口过程函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            HandleCreateMessage(hwnd);
            break;
        }
        
        case WM_SIZE: {
            HandleSizeMessage(hwnd, wParam, lParam);
            break;
        }
        
        case WM_SETCURSOR: {
            // 获取鼠标位置（屏幕坐标）
            POINT pt;
            GetCursorPos(&pt);
            // 转换为客户区坐标
            ScreenToClient(hwnd, &pt);
            
            // 检查是否在分隔条区域
            if (pt.x >= g_splitterPos && pt.x <= g_splitterPos + SPLITTER_WIDTH && pt.y > 50) {
                SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                return TRUE;
            }
            break; // 继续默认处理
        }
        
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            
            // 检查是否点击了分隔条 (在Y轴50像素以下)
            if (x >= g_splitterPos && x <= g_splitterPos + SPLITTER_WIDTH && y > 50) {
                g_isDraggingSplitter = TRUE;
                SetCapture(hwnd);
                SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                return 0;
            }
            break;
        }
        
        case WM_LBUTTONUP: {
            if (g_isDraggingSplitter) {
                g_isDraggingSplitter = FALSE;
                ReleaseCapture();
                return 0;
            }
            break;
        }
        
        case WM_MOUSEMOVE: {
            if (g_isDraggingSplitter) {
                int x = LOWORD(lParam);
                
                // 更新分隔条位置
                RECT rect;
                GetClientRect(hwnd, &rect);
                int clientWidth = rect.right - rect.left;
                
                // 限制拖动范围
                if (x < 100) x = 100;
                if (x > clientWidth - 100) x = clientWidth - 100;
                
                if (g_splitterPos != x) {
                    g_splitterPos = x;
                    // 强制调整布局
                    HandleSizeMessage(hwnd, 0, MAKELPARAM(rect.right, rect.bottom));
                    // 强制重绘
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            break;
        }
        
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDM_DEBUG) {
                HandleDebugCommand(hwnd, wParam);
            }
            
            if ((HWND)lParam == g_goButton) {
                if (HIWORD(wParam) == BN_CLICKED) {
                    HandleGoButtonClick(hwnd);
                }
            } else if ((HWND)lParam == g_backButton && HIWORD(wParam) == BN_CLICKED) {
                HandleBackButtonClick();
            } else if (LOWORD(wParam) == 1 || LOWORD(wParam) == 2 || LOWORD(wParam) == 3) {
                LogMessage(L"[DEBUG] WM_COMMAND 收到收藏夹命令: %d", LOWORD(wParam));
                HandleFavoriteCommands(wParam);
            }
            break;
        }
        
        case WM_NOTIFY: {
            // 处理通知消息
            return HandleNotificationMessages(hwnd, wParam, lParam);
        }
        break;
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // 绘制分隔线
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            // 创建灰色画笔
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            
            // 绘制线条
            int x = g_splitterPos + SPLITTER_WIDTH / 2;
            MoveToEx(hdc, x, 50, NULL);
            LineTo(hdc, x, rect.bottom);
            
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);
            
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY: {
            // 保存收藏夹数据
            LogMessage(L"[DEBUG] 保存收藏夹数据到文件...");
            saveFavoritesToFile();
            LogMessage(L"[DEBUG] 收藏夹数据保存完成，共 %d 项", g_favoriteCount);

            // 保存树展开状态
            LogMessage(L"[DEBUG] 保存树展开状态...");
            saveTreeExpansionState();
            
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}





// 处理后退按钮点击事件
void HandleBackButtonClick() {
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


// 处理收藏夹相关菜单命令
void HandleFavoriteCommands(WPARAM wParam) {
    if (LOWORD(wParam) == 1) {  // 移除收藏项菜单命令
        removeSelectedFavorite();
    } else if (LOWORD(wParam) == 2) {  // 添加当前路径到收藏夹菜单命令
        addCurrentPathToFavorites();
    } else if (LOWORD(wParam) == 3) {  // 编辑名称菜单命令
        editFavoriteName();
    }
}


// 处理ListView的双击消息
void HandleListViewDoubleClick(HWND hwnd, LPARAM lParam) {
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
}

// 处理调试命令
void HandleDebugCommand(HWND hwnd, WPARAM wParam) {
    HMENU hMenu = GetMenu(hwnd);
    
    // 检查控制台是否已分配
    if (IsConsoleApp()) {
        // 已分配，则关闭
        FreeConsole();
        CheckMenuItem(hMenu, IDM_DEBUG, MF_BYCOMMAND | MF_UNCHECKED);
        ShowCustomTooltip(hwnd, L"调试控制台已关闭");
    } else {
        // 未分配，则打开
        if (AllocateConsoleIfNeeded()) {
            CheckMenuItem(hMenu, IDM_DEBUG, MF_BYCOMMAND | MF_CHECKED);
            
            // 显示调试信息
            LogMessage(L"=== 调试信息 ===");
            LogMessage(L"主窗口句柄: 0x%p", hwnd);
            LogMessage(L"当前路径: %s", g_currentPath);
            LogMessage(L"收藏夹数量: %d", g_favoriteCount);
            
            // 显示收藏夹详情
            for (int i = 0; i < g_favoriteCount; i++) {
                LogMessage(L"  [%d] %s -> %s", i, g_favorites[i].name, g_favorites[i].path);
            }
            
            LogMessage(L"================");
            
            // 提示用户调试信息已在控制台输出
            ShowCustomTooltip(hwnd, L"调试控制台已开启");
        } else {
            MessageBoxW(hwnd, L"无法分配控制台窗口", L"错误", MB_OK | MB_ICONERROR);
        }
    }
}


// 注册窗口类
BOOL RegisterWindowClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ExplorerWindowClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    // 加载资源中的图标 (ID为1)
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(1), IMAGE_ICON, 
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    
    return RegisterClassExW(&wc) != 0;
}

// 创建主窗口
HWND CreateMainWindow(HINSTANCE hInstance) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 1024;
    int windowHeight = 768;
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        L"ExplorerWindowClass",
        L"我的资源管理器",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x, y, windowWidth, windowHeight,
        NULL, NULL, hInstance, NULL
    );
    
    if (hwnd) {
        // 创建菜单
        HMENU hMenu = CreateMenu();
        HMENU hHelpMenu = CreatePopupMenu();
        AppendMenuW(hHelpMenu, MF_STRING, IDM_DEBUG, L"Debug");
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, L"Help");
        SetMenu(hwnd, hMenu);

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

// 自定义提示窗口过程
LRESULT CALLBACK TooltipWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // 设置窗口背景色为浅黄色
            HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 220));
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush);
            return 0;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // 获取窗口文本
            WCHAR text[256] = {0};
            GetWindowTextW(hwnd, text, 256);
            
            // 绘制文本
            RECT rect;
            GetClientRect(hwnd, &rect);
            SetBkMode(hdc, TRANSPARENT);
            DrawTextW(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_TIMER: {
            if (wParam == 1) {
                KillTimer(hwnd, 1);
                ShowWindow(hwnd, SW_HIDE);
                g_tooltipWindow = NULL;
            }
            return 0;
        }
        
        case WM_DESTROY: {
            HBRUSH hBrush = (HBRUSH)GetClassLongPtr(hwnd, GCLP_HBRBACKGROUND);
            if (hBrush) DeleteObject(hBrush);
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// 处理TreeView的双击消息
void HandleTreeViewDoubleClick(HWND hwnd, HWND mainWindow) {
    // 获取选中的树节点
    HTREEITEM hSelectedItem = TreeView_GetSelection(g_treeView);
    if (hSelectedItem) {
        WCHAR fullPath[MAX_PATH] = {0};
        getNodeFullPath(g_treeView, hSelectedItem, fullPath, MAX_PATH);
        
        // 检查是否为收藏夹节点
        TVITEMW tvi = {0};
        tvi.hItem = hSelectedItem;
        tvi.mask = TVIF_PARAM;
        TreeView_GetItem(g_treeView, &tvi);
        
        // 检查是否为收藏夹根节点
        if (hSelectedItem == g_favoritesNode) {
            // 双击收藏夹根节点时不进行任何操作，允许其展开/折叠
            // 不调用setCurrentDirectory和updateFileList
            return;
        } else if (tvi.lParam == FAVORITE_ITEM_MARKER) {
            // 设置特殊路径标识
            lstrcpyW(g_currentPath, L"★ 收藏夹");
        } else if (tvi.lParam != 0 && tvi.lParam != (LPARAM)FAVORITE_ITEM_MARKER) {
            // 检查是否为收藏夹项（存储了指向FavoriteItem的指针）
            FavoriteItem* favoriteItem = (FavoriteItem*)tvi.lParam;
            if (favoriteItem >= g_favorites && favoriteItem < g_favorites + g_favoriteCount) {
                // 是收藏夹项，使用其存储的路径
                setCurrentDirectory(favoriteItem->path);
            } else {
                // 普通目录节点
                setCurrentDirectory(fullPath);
            }
        } else {
            // 普通目录节点
            setCurrentDirectory(fullPath);
        }
        updateFileList();
    }
}

// 处理调试命令显示自定义提示窗口
void ShowCustomTooltip(HWND parent, const WCHAR* text) {
    // 如果已经有一个提示窗口，先隐藏它
    if (g_tooltipWindow) {
        HideCustomTooltip();
    }
    
    // 注册提示窗口类（如果尚未注册）
    static BOOL classRegistered = FALSE;
    if (!classRegistered) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = TooltipWindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = TOOLTIP_WINDOW_CLASS;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        
        if (!RegisterClassW(&wc)) {
            return;
        }
        classRegistered = TRUE;
    }
    
    // 计算窗口大小
    HDC hdc = GetDC(parent);
    SIZE textSize;
    GetTextExtentPoint32W(hdc, text, wcslen(text), &textSize);
    ReleaseDC(parent, hdc);
    
    int width = textSize.cx + 20;
    int height = textSize.cy + 10;
    
    // 获取父窗口位置
    RECT parentRect;
    GetWindowRect(parent, &parentRect);
    
    // 计算提示窗口位置（居中于父窗口顶部）
    int x = parentRect.left + (parentRect.right - parentRect.left - width) / 2;
    int y = parentRect.top + 50;
    
    // 创建提示窗口
    g_tooltipWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        TOOLTIP_WINDOW_CLASS,
        text,
        WS_POPUP | WS_BORDER,
        x, y, width, height,
        parent, NULL, GetModuleHandle(NULL), NULL
    );
    
    if (g_tooltipWindow) {
        ShowWindow(g_tooltipWindow, SW_SHOW);
        UpdateWindow(g_tooltipWindow);
        
        // 设置3秒后自动关闭的定时器
        g_tooltipTimer = SetTimer(g_tooltipWindow, 1, 3000, NULL);
    }
}

// 隐藏自定义提示窗口
void HideCustomTooltip() {
    if (g_tooltipWindow) {
        KillTimer(g_tooltipWindow, g_tooltipTimer);
        ShowWindow(g_tooltipWindow, SW_HIDE);
        DestroyWindow(g_tooltipWindow);
        g_tooltipWindow = NULL;
    }
}

// WinMain入口点
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 分配控制台窗口并输出日志
    LogMessage(L"资源管理器程序启动中...");
    
    
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
    
    // 加载收藏夹数据
    LogMessage(L"开始加载收藏夹数据...");
    loadFavoritesFromFile();
    LogMessage(L"收藏夹数据加载完成，共 %d 项", g_favoriteCount);
    
    // 加载目录树
    LogMessage(L"开始加载目录树...");
    updateDirectoryTree();
    LogMessage(L"目录树加载完成");

    // 恢复树展开状态
    LogMessage(L"开始恢复树展开状态...");
    restoreTreeExpansionState();
    
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