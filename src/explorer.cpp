// 简化的C++资源管理器UI版本（Win32原生界面）
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wchar.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <string>
#include "favorites.h"
#include "notification_handlers.h"
#include "file_utils.h"
#include "tree_utils.h"
#include "log.h"
#include "settings.h"
#include "registry_integration.h"
#include "context_menu_manager.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// DropSource Implementation
class CDropSource : public IDropSource {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDropSource)) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release() {
        LONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }

    STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) {
        if (fEscapePressed) return DRAGDROP_S_CANCEL;
        if (!(grfKeyState & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }

    STDMETHODIMP GiveFeedback(DWORD dwEffect) { return DRAGDROP_S_USEDEFAULTCURSORS; }

    CDropSource() : m_cRef(1) {}
    virtual ~CDropSource() {}
private:
    LONG m_cRef;
};

// DataObject Implementation
class CDataObject : public IDataObject {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDataObject)) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release() {
        LONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }

    STDMETHODIMP GetData(FORMATETC *pformatetcIn, STGMEDIUM *pmedium) {
        if (!pformatetcIn || !pmedium) return E_INVALIDARG;
        
        if (pformatetcIn->cfFormat == CF_HDROP && (pformatetcIn->tymed & TYMED_HGLOBAL)) {
            // Calculate size needed
            size_t size = sizeof(DROPFILES);
            for (const auto& file : m_files) {
                size += (file.length() + 1) * sizeof(WCHAR);
            }
            size += sizeof(WCHAR); // Double null

            HGLOBAL hGlobal = GlobalAlloc(GHND, size);
            if (!hGlobal) return E_OUTOFMEMORY;

            DROPFILES* pDropFiles = (DROPFILES*)GlobalLock(hGlobal);
            pDropFiles->pFiles = sizeof(DROPFILES);
            pDropFiles->pt.x = 0;
            pDropFiles->pt.y = 0;
            pDropFiles->fNC = FALSE;
            pDropFiles->fWide = TRUE;

            WCHAR* pPath = (WCHAR*)((BYTE*)pDropFiles + sizeof(DROPFILES));
            for (const auto& file : m_files) {
                lstrcpyW(pPath, file.c_str());
                pPath += file.length() + 1;
            }
            *pPath = 0; // Final null

            GlobalUnlock(hGlobal);

            pmedium->tymed = TYMED_HGLOBAL;
            pmedium->hGlobal = hGlobal;
            pmedium->pUnkForRelease = NULL;
            return S_OK;
        }
        return DV_E_FORMATETC;
    }

    STDMETHODIMP GetDataHere(FORMATETC *pformatetc, STGMEDIUM *pmedium) { return E_NOTIMPL; }
    STDMETHODIMP QueryGetData(FORMATETC *pformatetc) {
        if (pformatetc->cfFormat == CF_HDROP && (pformatetc->tymed & TYMED_HGLOBAL)) return S_OK;
        return DV_E_FORMATETC;
    }
    STDMETHODIMP GetCanonicalFormatEtc(FORMATETC *pformatectIn, FORMATETC *pformatetcOut) { return E_NOTIMPL; }
    STDMETHODIMP SetData(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease) { return E_NOTIMPL; }
    
    STDMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc) {
        if (dwDirection == DATADIR_GET) {
            FORMATETC fmt = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
            return SHCreateStdEnumFmtEtc(1, &fmt, ppenumFormatEtc);
        }
        return E_NOTIMPL;
    }
    
    STDMETHODIMP DAdvise(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection) { return OLE_E_ADVISENOTSUPPORTED; }
    STDMETHODIMP DUnadvise(DWORD dwConnection) { return OLE_E_ADVISENOTSUPPORTED; }
    STDMETHODIMP EnumDAdvise(IEnumSTATDATA **ppenumAdvise) { return OLE_E_ADVISENOTSUPPORTED; }

    CDataObject(const std::vector<std::wstring>& files) : m_cRef(1), m_files(files) {}
    virtual ~CDataObject() {}
private:
    LONG m_cRef;
    std::vector<std::wstring> m_files;
};

#define IDM_DEBUG 1001
#define IDM_STATUSBAR 1002
#define IDM_NEW_TAB 1003
#define IDM_CLOSE_TAB 1004
#define IDM_NEW_WINDOW 1005
#define IDM_TILE_WINDOWS 1006
#define IDM_NEW_DISK_DETAILS 1007
#define IDM_SET_DEFAULT 1008
#define IDM_RESTORE_DEFAULT 1009
#define IDM_CONTEXT_MENU_MGR 1010
#define ID_BTN_NEW_TAB 2001
#define WM_APP_DIRSIZE (WM_APP + 1)
#define WM_APP_LISTITEM (WM_APP + 2)
#define WM_APP_LISTDONE (WM_APP + 3)
#define WM_APP_UPDATE_COUNT (WM_APP + 4)
#define WM_APP_SORTDONE (WM_APP + 5)
#define IDT_UI_BATCH 100


struct DirSizeResult {
    WCHAR parent[MAX_PATH];
    WCHAR name[MAX_PATH];
    ULONGLONG size;
    BOOL isPartial;
};

struct DirSizeTask {
    WCHAR parent[MAX_PATH];
    std::vector<std::wstring> names;
    LONG generation;
};

struct ListEnumTask {
    WCHAR parent[MAX_PATH];
    LONG generation;
};

struct ListItemResult {
    WCHAR parent[MAX_PATH];
    WCHAR name[MAX_PATH];
    BOOL isDir;
    ULONGLONG size;
    FILETIME created;
    FILETIME modified;
};


DWORD WINAPI DirSizeWorker(LPVOID lpParam);
void UpdateListViewDirSize(const WCHAR* parentPath, const WCHAR* name, ULONGLONG size, BOOL isPartial);
DWORD WINAPI ListEnumWorker(LPVOID lpParam);

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
int g_diskScrollY = 0; // 磁盘详情页滚动位置
HWND g_treeView = NULL;  // 左侧目录树
HWND g_listView = NULL;  // 右侧文件列表
HWND g_addressBar = NULL;
HWND g_goButton = NULL;
HWND g_backButton = NULL;
HWND g_forwardButton = NULL;
HWND g_upButton = NULL;
HWND g_newTabButton = NULL;
HWND g_openInExplorerButton = NULL;
HWND g_settingsButton = NULL;
HWND g_addFavoriteButton = NULL;  // 添加收藏按钮
HWND g_statusBar = NULL;  // 底部状态栏
double g_diskUsageRatio = 0.0;  // 磁盘占用比例 (0.0 - 1.0)
WCHAR g_diskSpaceInfo[256] = {0};  // 磁盘空间信息文本
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
static LONG g_dirSizeGen = 0;
CRITICAL_SECTION g_fileListLock;
std::vector<ItemSortData> g_fileList;
BOOL g_enumInProgress = FALSE;
BOOL g_timerActive = FALSE;
BOOL g_sorting = FALSE;
BOOL g_isNavigatingFromTree = FALSE;

// Tab management
struct TabInfo {
    WCHAR path[MAX_PATH];
    std::vector<ItemSortData> fileList;
};
std::vector<TabInfo> g_tabs;
int g_currentTabIndex = -1;
HWND g_tabCtrl = NULL;

// Drag and Drop Globals
 BOOL g_isDragging = FALSE;
 std::vector<std::wstring> g_draggedFiles;
 HCURSOR g_hCursorCopy = NULL;
 HCURSOR g_hCursorMove = NULL;
 int g_hoverTab = -1;
 DWORD g_hoverStartTime = 0;
 
 void updateFileList(); // Forward declaration

 // Helper to perform file copy/move
void PerformFileOperation(const std::vector<std::wstring>& files, const WCHAR* destPath, BOOL isMove) {
    if (files.empty() || !destPath || !*destPath) return;

    // Double-null terminated string for SHFileOperation
    size_t totalLen = 0;
    for (const auto& file : files) totalLen += file.length() + 1;
    totalLen += 1; // Final null

    std::vector<WCHAR> sourceBuffer(totalLen, 0);
    WCHAR* p = sourceBuffer.data();
    for (const auto& file : files) {
        lstrcpyW(p, file.c_str());
        p += file.length() + 1;
    }

    SHFILEOPSTRUCTW sfo = {0};
    sfo.hwnd = g_mainWindow;
    sfo.wFunc = isMove ? FO_MOVE : FO_COPY;
    sfo.pFrom = sourceBuffer.data();
    sfo.pTo = destPath;
    sfo.fFlags = FOF_ALLOWUNDO | FOF_SIMPLEPROGRESS;

    SHFileOperationW(&sfo);
    
    // Refresh
    updateFileList();
}

// 函数声明
void HandleGoButtonClick(HWND hwnd);
void updateFileList();
void SwitchToTab(int index);
void HandleSizeMessage(HWND hwnd, WPARAM wParam, LPARAM lParam);
void AddNewTab(const WCHAR* path);
void CloseTab(int index);
void HandleBackButtonClick();
void HandleFavoriteCommands(WPARAM wParam);
void HandleListViewDoubleClick(HWND hwnd, LPARAM lParam);
void HandleTreeViewDoubleClick(HWND hwnd, HWND mainWindow);
void HandleDebugCommand(HWND hwnd, WPARAM wParam);
BOOL RegisterWindowClass(HINSTANCE hInstance);
HWND CreateMainWindow(HINSTANCE hInstance);
LRESULT CALLBACK StatusBarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);  // 状态栏自定义绘制过程

// 获取当前工作目录
void getCurrentDirectory(WCHAR* buffer, DWORD bufferSize) {
    GetCurrentDirectoryW(bufferSize, buffer);
}

// 设置当前工作目录
void setCurrentDirectory(const WCHAR* path) {
    SetCurrentDirectoryW(path);
    lstrcpyW(g_currentPath, path);
    updateDiskUsageRatio(path);
    InterlockedIncrement(&g_dirSizeGen);
    
    // 更新地址栏
    if (g_addressBar) {
        SetWindowTextW(g_addressBar, path);
    }

    // Update Tab Title
    if (g_currentTabIndex >= 0 && g_tabCtrl) {
        const WCHAR* p = wcsrchr(path, L'\\');
        const WCHAR* name = p ? p + 1 : path;
        if (!*name) name = path; // Root like C:\\
        if (!*name) name = L"此电脑";
        // If "磁盘详情", keep it
        if (wcscmp(path, L"磁盘详情") == 0) name = L"磁盘详情";
        
        TCITEMW tie;
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)name;
        TabCtrl_SetItem(g_tabCtrl, g_currentTabIndex, &tie);
        
        // Update internal tab state path
        lstrcpyW(g_tabs[g_currentTabIndex].path, path);
    }
    
    // Sync TreeView if not navigating from it
    if (!g_isNavigatingFromTree) {
        syncTreeViewWithPath(path);
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
    
    // 如果是右键菜单管理页面，不进行文件列表更新
    if (wcscmp(g_currentPath, L"右键菜单管理") == 0) {
        return;
    }

    if (!g_listView) {
        LogMessage(L"[DEBUG] g_listView 为空，跳过更新");
        return;
    }

    BOOL isDriveView = (g_currentPath[0] == L'\0' || wcscmp(g_currentPath, L"此电脑") == 0 || wcscmp(g_currentPath, L"磁盘详情") == 0);
    
    // Setup Columns based on view type
    LVCOLUMNW col = {0};
    col.mask = LVCF_TEXT;
    
    if (isDriveView) {
        col.pszText = (LPWSTR)L"磁盘";
        SendMessageW(g_listView, LVM_SETCOLUMNW, 0, (LPARAM)&col);

        col.pszText = (LPWSTR)L"总大小";
        SendMessageW(g_listView, LVM_SETCOLUMNW, 1, (LPARAM)&col);
        SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 1, 150); // Total Size Width
        
        col.pszText = (LPWSTR)L"使用情况"; // Was Type
        SendMessageW(g_listView, LVM_SETCOLUMNW, 2, (LPARAM)&col);
        SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 2, 200); // Usage Width (enough for 120px bar + text)

        col.pszText = (LPWSTR)L"可用空间";
        SendMessageW(g_listView, LVM_SETCOLUMNW, 3, (LPARAM)&col);
        SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 3, 150); // Free Space Width
        
        // Hide Created Time Column
        SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 4, 0);
    } else {
        col.pszText = (LPWSTR)L"名称";
        SendMessageW(g_listView, LVM_SETCOLUMNW, 0, (LPARAM)&col);
    
        col.pszText = (LPWSTR)L"大小";
        SendMessageW(g_listView, LVM_SETCOLUMNW, 1, (LPARAM)&col);
        SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 1, 150); // Restore default
        
        col.pszText = (LPWSTR)L"类型";
        SendMessageW(g_listView, LVM_SETCOLUMNW, 2, (LPARAM)&col);
        SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 2, 100); // Restore default
    
        col.pszText = (LPWSTR)L"修改时间";
        SendMessageW(g_listView, LVM_SETCOLUMNW, 3, (LPARAM)&col);
        SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 3, 150); // Restore default
    
        col.pszText = (LPWSTR)L"创建时间";
        SendMessageW(g_listView, LVM_SETCOLUMNW, 4, (LPARAM)&col);
        SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 4, 150); // Restore default
    }
    
    // Handle Drive List (Empty Path or "此电脑" or "磁盘详情")
    if (isDriveView) {
        SendMessageW(g_listView, LVM_SETITEMCOUNT, 0, LVSICF_NOINVALIDATEALL);
        InvalidateRect(g_listView, NULL, TRUE);
        UpdateWindow(g_listView);
        
        EnterCriticalSection(&g_fileListLock);
        g_fileList.clear();
        
        DriveInfo drives[26];
        int count = getDriveList(drives, 26);
        for (int i=0; i<count; i++) {
            ItemSortData item;
            item.name = std::wstring(drives[i].letter); // "C:\"
            if (drives[i].volName[0]) {
                item.name += L" (";
                item.name += drives[i].volName;
                item.name += L")";
            }
            item.isDir = TRUE; 
            item.sizeNumeric = drives[i].totalBytes.QuadPart;
            item.freeSpace = drives[i].freeBytes.QuadPart;
            item.totalSpace = drives[i].totalBytes.QuadPart;
            item.created.dwLowDateTime = 0;
            item.created.dwHighDateTime = 0;
            item.modified.dwLowDateTime = 0;
            item.modified.dwHighDateTime = 0;
            g_fileList.push_back(item);
        }
        LeaveCriticalSection(&g_fileListLock);
        
        SendMessageW(g_listView, LVM_SETITEMCOUNT, (WPARAM)g_fileList.size(), LVSICF_NOINVALIDATEALL);
        InvalidateRect(g_listView, NULL, TRUE);
        
        // Update Tab Title
        if (g_currentTabIndex >= 0 && g_tabCtrl) {
             TCITEMW tie;
             tie.mask = TCIF_TEXT;
             tie.pszText = (LPWSTR)(wcscmp(g_currentPath, L"磁盘详情") == 0 ? L"磁盘详情" : L"此电脑");
             TabCtrl_SetItem(g_tabCtrl, g_currentTabIndex, &tie);
        }
        g_enumInProgress = FALSE;
        return;
    }

    LogMessage(L"[DEBUG] 清空ListView现有项目");
    // Virtual List: Reset count to 0
    SendMessageW(g_listView, LVM_SETITEMCOUNT, 0, LVSICF_NOINVALIDATEALL);
    InvalidateRect(g_listView, NULL, TRUE);
    UpdateWindow(g_listView);
    
    EnterCriticalSection(&g_fileListLock);
    g_fileList.clear();
    LeaveCriticalSection(&g_fileListLock);

    g_enumInProgress = TRUE;
    if (g_timerActive) {
        KillTimer(g_mainWindow, IDT_UI_BATCH);
        g_timerActive = FALSE;
    }
    
    // 检查当前路径是否为收藏夹路径的特殊情况
    BOOL isFavoritesPath = FALSE;
    if (wcsncmp(g_currentPath, L"★ 收藏夹", 5) == 0) {
        isFavoritesPath = TRUE;
        LogMessage(L"[DEBUG] 当前为收藏夹路径");
    }
    
    if (isFavoritesPath) {
        // 显示收藏夹项作为文件列表
        EnterCriticalSection(&g_fileListLock);
        for (int i = 0; i < g_favoriteCount; ++i) {
            ItemSortData item;
            item.name = g_favorites[i].name;
            item.isDir = TRUE;
            item.sizeNumeric = 0;
            item.created.dwLowDateTime = 0;
            item.created.dwHighDateTime = 0;
            item.modified.dwLowDateTime = 0;
            item.modified.dwHighDateTime = 0;
            g_fileList.push_back(item);
        }
        LeaveCriticalSection(&g_fileListLock);

        SendMessageW(g_listView, LVM_SETITEMCOUNT, (WPARAM)g_favoriteCount, LVSICF_NOINVALIDATEALL);
        InvalidateRect(g_listView, NULL, TRUE);
        
        LogMessage(L"[DEBUG] updateFileList 完成，共添加 %d 个收藏夹项目", g_favoriteCount);
        g_enumInProgress = FALSE;
    } else {
        // 异步枚举目录项
        ListEnumTask* t = new ListEnumTask();
        lstrcpyW(t->parent, g_currentPath);
        t->generation = g_dirSizeGen;
        CreateThread(NULL, 0, ListEnumWorker, t, 0, NULL);
        LogMessage(L"[DEBUG] 列表异步枚举已启动");
    }
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

// 创建硬盘图标（使用系统图标）
HICON createDriveIcon() {
    // 使用系统驱动器图标
    SHFILEINFO sfi = {0};
    SHGetFileInfo(TEXT("C:\\"), 0, &sfi, sizeof(sfi), 
                  SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    return sfi.hIcon;
}

// 创建收藏夹图标（使用ICO文件）
HICON createFavoriteIcon() {
    // 从ICO文件加载收藏夹图标（从bin目录）
    WCHAR iconPath[MAX_PATH];
    GetModuleFileNameW(NULL, iconPath, MAX_PATH);
    WCHAR* lastSlash = wcsrchr(iconPath, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
        lstrcatW(iconPath, L"favorite_icon.ico");
    }
    
    HICON hIcon = (HICON)LoadImageW(
        NULL,
        iconPath,
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_LOADFROMFILE | LR_DEFAULTSIZE
    );
    
    // 如果加载失败，使用系统文件夹图标作为备用
    if (!hIcon) {
        SHFILEINFO sfi = {0};
        SHGetFileInfoW(L"", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi), 
                      SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
        hIcon = sfi.hIcon;
    }
    
    return hIcon;
}

// Tab helper functions
void SaveCurrentTabState() {
    if (g_currentTabIndex >= 0 && g_currentTabIndex < (int)g_tabs.size()) {
        lstrcpyW(g_tabs[g_currentTabIndex].path, g_currentPath);
        EnterCriticalSection(&g_fileListLock);
        g_tabs[g_currentTabIndex].fileList = g_fileList;
        LeaveCriticalSection(&g_fileListLock);
    }
}

void SwitchToTab(int index) {
    if (index < 0 || index >= (int)g_tabs.size()) return;
    
    // Save current if switching from a valid tab
    if (g_currentTabIndex >= 0) {
        SaveCurrentTabState();
    }
    
    // Switch
    g_currentTabIndex = index;
    if (g_tabCtrl) TabCtrl_SetCurSel(g_tabCtrl, index);
    
    // Restore
    lstrcpyW(g_currentPath, g_tabs[index].path);

    // Check if it's the Context Menu Manager tab
    if (wcscmp(g_currentPath, L"右键菜单管理") == 0) {
        ShowContextMenuManager(TRUE);
        if (g_listView) ShowWindow(g_listView, SW_HIDE);
        if (g_treeView) ShowWindow(g_treeView, SW_HIDE);
        if (g_addressBar) SetWindowTextW(g_addressBar, L"右键菜单管理");
        
        // Force layout update
        if (g_tabCtrl) {
            HWND hwnd = GetParent(g_tabCtrl);
            if (hwnd) {
                 RECT rc;
                 GetClientRect(hwnd, &rc);
                 HandleSizeMessage(hwnd, 0, MAKELPARAM(rc.right, rc.bottom));
            }
        }
        return;
    } else {
        ShowContextMenuManager(FALSE);
        if (g_listView) ShowWindow(g_listView, SW_SHOW);
        // TreeView visibility will be handled by HandleSizeMessage
    }

    EnterCriticalSection(&g_fileListLock);
    g_fileList = g_tabs[index].fileList;
    LeaveCriticalSection(&g_fileListLock);
    
    // Update UI
    if (g_addressBar) SetWindowTextW(g_addressBar, g_currentPath);
    if (g_listView) {
        SendMessageW(g_listView, LVM_SETITEMCOUNT, (WPARAM)g_fileList.size(), LVSICF_NOINVALIDATEALL);
        InvalidateRect(g_listView, NULL, TRUE);
        UpdateWindow(g_listView);
    }
    
    // If empty list and empty path, populate drive list
    if (g_fileList.empty() && (g_currentPath[0] == L'\0' || wcscmp(g_currentPath, L"此电脑") == 0 || wcscmp(g_currentPath, L"磁盘详情") == 0)) {
        updateFileList();
    } else {
        // Even if we don't update file list, we MUST update columns based on current path
        // because we might be switching from a Drive View tab to a Normal View tab or vice versa
        BOOL isDriveView = (g_currentPath[0] == L'\0' || wcscmp(g_currentPath, L"此电脑") == 0 || wcscmp(g_currentPath, L"磁盘详情") == 0);
        
        LVCOLUMNW col = {0};
        col.mask = LVCF_TEXT;
        
        if (isDriveView) {
            col.pszText = (LPWSTR)L"磁盘";
            SendMessageW(g_listView, LVM_SETCOLUMNW, 0, (LPARAM)&col);

            col.pszText = (LPWSTR)L"总大小";
            SendMessageW(g_listView, LVM_SETCOLUMNW, 1, (LPARAM)&col);
            SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 1, 150); 
            
            col.pszText = (LPWSTR)L"使用情况"; 
            SendMessageW(g_listView, LVM_SETCOLUMNW, 2, (LPARAM)&col);
            SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 2, 200);

            col.pszText = (LPWSTR)L"可用空间";
            SendMessageW(g_listView, LVM_SETCOLUMNW, 3, (LPARAM)&col);
            SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 3, 150); 
            
            SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 4, 0);
        } else {
            col.pszText = (LPWSTR)L"名称";
            SendMessageW(g_listView, LVM_SETCOLUMNW, 0, (LPARAM)&col);
        
            col.pszText = (LPWSTR)L"大小";
            SendMessageW(g_listView, LVM_SETCOLUMNW, 1, (LPARAM)&col);
            SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 1, 150); 
            
            col.pszText = (LPWSTR)L"类型";
            SendMessageW(g_listView, LVM_SETCOLUMNW, 2, (LPARAM)&col);
            SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 2, 100); 
        
            col.pszText = (LPWSTR)L"修改时间";
            SendMessageW(g_listView, LVM_SETCOLUMNW, 3, (LPARAM)&col);
            SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 3, 150); 
        
            col.pszText = (LPWSTR)L"创建时间";
            SendMessageW(g_listView, LVM_SETCOLUMNW, 4, (LPARAM)&col);
            SendMessageW(g_listView, LVM_SETCOLUMNWIDTH, 4, 150); 
            
            // Sync TreeView
            syncTreeViewWithPath(g_currentPath);
        }
    }
    
    // Force layout update to hide/show TreeView based on new path
    if (g_tabCtrl) {
        HWND hwnd = GetParent(g_tabCtrl);
        if (hwnd) {
             RECT rc;
             GetClientRect(hwnd, &rc);
             HandleSizeMessage(hwnd, 0, MAKELPARAM(rc.right, rc.bottom));
        }
    }
}

void AddNewTab(const WCHAR* path) {
    TabInfo info;
    lstrcpyW(info.path, path ? path : L"");
    g_tabs.push_back(info);
    
    int newIndex = (int)g_tabs.size() - 1;
    
    if (g_tabCtrl) {
        TCITEMW tie;
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)(path && *path ? path : L"此电脑");
        TabCtrl_InsertItem(g_tabCtrl, newIndex, &tie);
    }
    
    SwitchToTab(newIndex);
}

void CloseTab(int index) {
    if (index < 0 || index >= (int)g_tabs.size()) return;
    if (g_tabs.size() <= 1) return; // Don't close last tab
    
    g_tabs.erase(g_tabs.begin() + index);
    if (g_tabCtrl) TabCtrl_DeleteItem(g_tabCtrl, index);
    
    if (g_currentTabIndex >= index) {
        g_currentTabIndex--;
    }
    if (g_currentTabIndex < 0) g_currentTabIndex = 0;
    
    SwitchToTab(g_currentTabIndex);
}

void HandleNewWindow() {
    WCHAR exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    
    if (CreateProcessW(exePath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

BOOL CALLBACK TileWindowsEnumProc(HWND hwnd, LPARAM lParam) {
    std::vector<HWND>* windows = (std::vector<HWND>*)lParam;
    WCHAR className[256];
    if (GetClassNameW(hwnd, className, 256)) {
        if (wcscmp(className, L"ExplorerWindowClass") == 0 && IsWindowVisible(hwnd)) {
            windows->push_back(hwnd);
        }
    }
    return TRUE;
}

void HandleTileWindows() {
    std::vector<HWND> windows;
    EnumWindows(TileWindowsEnumProc, (LPARAM)&windows);
    
    // Smart logic: If only 1 window but multiple tabs, split the current tab into a new window
    if (windows.size() == 1 && g_tabs.size() > 1) {
        // Get current path
        WCHAR cmdLine[MAX_PATH + 20];
        wsprintfW(cmdLine, L"\"%s\"", g_currentPath); // Quote path
        
        // Launch new process
        WCHAR exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.cbSize = sizeof(sei);
        sei.lpFile = exePath;
        sei.lpParameters = cmdLine;
        sei.nShow = SW_SHOWNORMAL;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        ShellExecuteExW(&sei);
        
        // Close current tab in this window
        CloseTab(g_currentTabIndex);
        
        // Wait for new window to appear (simple retry loop)
        for (int i = 0; i < 15; i++) {
            Sleep(200);
            windows.clear();
            EnumWindows(TileWindowsEnumProc, (LPARAM)&windows);
            if (windows.size() > 1) break;
        }
    }

    if (windows.empty()) return;
    
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    
    int width = (workArea.right - workArea.left) / (int)windows.size();
    int height = workArea.bottom - workArea.top;
    
    for (size_t i = 0; i < windows.size(); i++) {
        SetWindowPos(windows[i], NULL, 
            workArea.left + (int)i * width, workArea.top, 
            width, height, 
            SWP_NOZORDER | SWP_SHOWWINDOW);
    }
}

// 处理WM_CREATE消息的函数
void HandleCreateMessage(HWND hwnd) {
    InitializeCriticalSection(&g_fileListLock);
    // 初始化通用控件
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
    
    // 创建自定义字体，使用设置中的字体大小
    int fontSize = getFontSize();
    LOGFONTW lf = {0};
    lf.lfHeight = -MulDiv(fontSize, GetDeviceCaps(GetDC(hwnd), LOGPIXELSY), 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    lstrcpyW(lf.lfFaceName, L"Microsoft YaHei"); // 使用微软雅黑字体以更好地支持中文
    
    HFONT hFont = CreateFontIndirectW(&lf);
    if (!hFont) {
        hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }
    if (hFont) {
        SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    }
    
    // Create separate font for tabs (smaller)
    LOGFONTW lfTab = lf;
    // Use a slightly smaller font for tabs, or fixed 9pt if main font is large
    int tabFontSize = fontSize > 10 ? fontSize - 2 : fontSize;
    if (tabFontSize > 9) tabFontSize = 9; // Cap at 9pt for compact tabs
    
    lfTab.lfHeight = -MulDiv(tabFontSize, GetDeviceCaps(GetDC(hwnd), LOGPIXELSY), 72);
    HFONT hTabFont = CreateFontIndirectW(&lfTab);
    
    // 创建Tab Control
    g_tabCtrl = CreateWindowExW(
        0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_FOCUSNEVER,
        0, 0, 800, 25,
        hwnd, NULL, NULL, NULL
    );
    if (hTabFont) {
        SendMessage(g_tabCtrl, WM_SETFONT, (WPARAM)hTabFont, MAKELPARAM(TRUE, 0));
    } else if (hFont) {
        SendMessage(g_tabCtrl, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    }
    // Initialize first tab
    // 使用启动参数中指定的路径（如果有），否则默认为"此电脑"
    if (wcslen(g_currentPath) > 0 && wcscmp(g_currentPath, L"此电脑") != 0) {
        AddNewTab(g_currentPath);
    } else {
        AddNewTab(L"");
    }
    AddNewTab(L"磁盘详情");
    
    // 默认切换回第一个标签页 ("此电脑")
    SwitchToTab(0);

    // 创建后退按钮
    g_backButton = CreateWindowExW(
        0, L"BUTTON", L"←",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 10, 40, 25,
        hwnd, NULL, NULL, NULL
    );
    
    // 创建前进按钮
    g_forwardButton = CreateWindowExW(
        0, L"BUTTON", L"→",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        55, 10, 40, 25,
        hwnd, NULL, NULL, NULL
    );

    // 创建向上按钮
    g_upButton = CreateWindowExW(
        0, L"BUTTON", L"↑",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        100, 10, 40, 25,
        hwnd, NULL, NULL, NULL
    );

    // 创建新建标签页按钮
    g_newTabButton = CreateWindowExW(
        0, L"BUTTON", L"+",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        145, 10, 25, 25,
        hwnd, (HMENU)ID_BTN_NEW_TAB, NULL, NULL
    );

    // 创建地址栏
    g_addressBar = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        175, 10, 425, 25,
        hwnd, NULL, NULL, NULL
    );
    
    // 子类化地址栏以处理回车键
    if (g_addressBar) {
        g_OriginalAddressBarProc = (WNDPROC)SetWindowLongPtr(g_addressBar, GWLP_WNDPROC, (LONG_PTR)AddressBarProc);
    }
    
    // 创建前往按钮
    g_goButton = CreateWindowExW(
        0, L"BUTTON", L"前往",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        610, 10, 60, 25,
        hwnd, NULL, NULL, NULL
    );

    // 创建Open Explorer按钮
    g_openInExplorerButton = CreateWindowExW(
        0, L"BUTTON", L"打开",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        680, 10, 60, 25,
        hwnd, NULL, NULL, NULL
    );

    // 创建设置按钮
    g_settingsButton = CreateWindowExW(
        0, L"BUTTON", L"设置",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        750, 10, 60, 25,
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
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_OWNERDATA,
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

    // 创建硬盘图标
    HICON hHardDriveIcon = createDriveIcon();

    // 创建收藏夹图标（星号★）
    HICON hFavoriteIcon = createFavoriteIcon();

    // 创建图像列表用于TreeView - 使用系统主题适配的图标尺寸
    HIMAGELIST hTreeImageList = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 
        ILC_COLOR32 | ILC_MASK, 4, 10);
    // 设置图像列表的背景色为白色
    ImageList_SetBkColor(hTreeImageList, RGB(255, 255, 255));
    // 添加图标
    ImageList_AddIcon(hTreeImageList, hFolderIcon);
    ImageList_AddIcon(hTreeImageList, hDriveIcon);
    ImageList_AddIcon(hTreeImageList, hHardDriveIcon);
    ImageList_AddIcon(hTreeImageList, hFavoriteIcon);
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
    DestroyIcon(hHardDriveIcon);
    DestroyIcon(hFavoriteIcon);
    
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

    LVCOLUMNW col4 = {0};
    col4.mask = LVCF_TEXT | LVCF_WIDTH;
    col4.cx = 150;
    col4.pszText = (LPWSTR)L"修改时间";
    SendMessageW(g_listView, LVM_INSERTCOLUMNW, 3, (LPARAM)&col4);

    LVCOLUMNW col5 = {0};
    col5.mask = LVCF_TEXT | LVCF_WIDTH;
    col5.cx = 150;
    col5.pszText = (LPWSTR)L"创建时间";
    SendMessageW(g_listView, LVM_INSERTCOLUMNW, 4, (LPARAM)&col5);
    
    // 创建状态栏（底部）
    g_statusBar = CreateWindowExW(
        0,
        STATUSCLASSNAMEW,
        L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hwnd,
        (HMENU)IDM_STATUSBAR,
        GetModuleHandle(NULL),
        NULL
    );
    
    // 设置状态栏高度为35像素
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    SetWindowPos(g_statusBar, NULL, 0, rcClient.bottom - 35, rcClient.right, 35, SWP_NOZORDER);
    
    // 子类化状态栏以实现自定义绘制
    SetWindowLongPtr(g_statusBar, GWLP_WNDPROC, (LONG_PTR)StatusBarProc);
    
    // 设置状态栏的初始文本
    SendMessageW(g_statusBar, WM_SETTEXT, 0, (LPARAM)L"就绪");
    
    // 初始化右键菜单管理器
    InitContextMenuManager(hwnd);
}

// 处理WM_SIZE消息的函数
void HandleSizeMessage(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    // 忽略最小化消息
    if (wParam == SIZE_MINIMIZED) {
        return;
    }

    int clientWidth = LOWORD(lParam);
    int clientHeight = HIWORD(lParam);

    // 调整Tab Control大小
    if (g_tabCtrl) MoveWindow(g_tabCtrl, 0, 0, clientWidth, 25, TRUE);
    
    int topOffset = 30; // Tab高度(25) + 间距(5)

    // 调整控件大小
    if (g_backButton) MoveWindow(g_backButton, 10, topOffset, 40, 25, TRUE);
    if (g_forwardButton) MoveWindow(g_forwardButton, 55, topOffset, 40, 25, TRUE);
    if (g_upButton) MoveWindow(g_upButton, 100, topOffset, 40, 25, TRUE);
    if (g_newTabButton) MoveWindow(g_newTabButton, 145, topOffset, 25, 25, TRUE);
    
    int settingsBtnX = clientWidth - 70;
    int openBtnX = clientWidth - 135;
    int goBtnX = clientWidth - 200;

    if (g_goButton) MoveWindow(g_goButton, goBtnX, topOffset, 60, 25, TRUE);
    if (g_openInExplorerButton) MoveWindow(g_openInExplorerButton, openBtnX, topOffset, 60, 25, TRUE);
    if (g_settingsButton) MoveWindow(g_settingsButton, settingsBtnX, topOffset, 60, 25, TRUE);

    if (g_addressBar) {
        int addrWidth = goBtnX - 175 - 10;
        if (addrWidth < 0) addrWidth = 0;
        MoveWindow(g_addressBar, 175, topOffset, addrWidth, 25, TRUE);
    }
    
    // 确保分隔条位置在合理范围内
    if (g_splitterPos < 100) g_splitterPos = 100;
    if (g_splitterPos > clientWidth - 100) g_splitterPos = clientWidth - 100;
    
    int contentY = topOffset + 35; // 工具栏高度(25) + 间距(10)

    // Handle Context Menu Manager resizing
    if (wcscmp(g_currentPath, L"右键菜单管理") == 0) {
         RECT rcContent;
         rcContent.left = 0;
         rcContent.right = clientWidth;
         rcContent.top = contentY;
         rcContent.bottom = clientHeight - 35; // Status bar
         ResizeContextMenuManager(rcContent);
         
         // Ensure others are hidden
         if (g_treeView) ShowWindow(g_treeView, SW_HIDE);
         if (g_listView) ShowWindow(g_listView, SW_HIDE);
         
         // Adjust status bar
         if (g_statusBar) {
            SetWindowPos(g_statusBar, NULL, 0, clientHeight - 35, clientWidth, 35, SWP_NOZORDER);
         }
         return;
    }

    BOOL isDiskDetails = (wcscmp(g_currentPath, L"磁盘详情") == 0);

    // 调整TreeView大小 (左侧目录树)
    if (g_treeView) {
        if (isDiskDetails) {
            ShowWindow(g_treeView, SW_HIDE);
        } else {
            ShowWindow(g_treeView, SW_SHOW);
            int treeHeight = clientHeight - contentY - 35;
            if (treeHeight < 0) treeHeight = 0;
            MoveWindow(g_treeView, 10, contentY, g_splitterPos - 10, treeHeight, TRUE);
        }
    }
    
    // 调整ListView大小 (右侧文件列表)
    if (g_listView) {
        int listHeight = clientHeight - contentY - 35;
        
        // 如果是磁盘详情页，完全隐藏 ListView，预留全部空间给自定义绘制
        if (isDiskDetails) {
            MoveWindow(g_listView, 0, 0, 0, 0, TRUE);
            ShowWindow(g_listView, SW_HIDE);
        } else {
            ShowWindow(g_listView, SW_SHOW);
            
            int listViewX = g_splitterPos + SPLITTER_WIDTH;
            int listViewWidth = clientWidth - listViewX - 10;
            if (listViewWidth < 0) listViewWidth = 0;
            if (listHeight < 0) listHeight = 0;
            
            MoveWindow(g_listView, listViewX, contentY, listViewWidth, listHeight, TRUE);
        }
    }
    
    // 调整状态栏大小（底部）
    if (g_statusBar) {
        SetWindowPos(g_statusBar, NULL, 0, clientHeight - 35, clientWidth, 35, SWP_NOZORDER);
    }

    // 如果是磁盘详情页，强制重绘整个窗口以更新布局
    if (isDiskDetails) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

// 保存布局状态
void saveLayoutState() {
    // 获取可执行文件目录
    WCHAR exePath[MAX_PATH];
    getExecutableDirectory(exePath, MAX_PATH);
    
    // 构造完整路径
    WCHAR filePath[MAX_PATH];
    lstrcpyW(filePath, exePath);
    lstrcatW(filePath, L"layout_state.ini");
    
    FILE* fp = NULL;
    errno_t err = _wfopen_s(&fp, filePath, L"w, ccs=UTF-8");
    if (err == 0 && fp) {
        fwprintf(fp, L"SplitterPos=%d\n", g_splitterPos);
        fclose(fp);
    }
}

void HandleDestroyMessage(HWND hwnd) {
    if (g_timerActive) {
        KillTimer(hwnd, IDT_UI_BATCH);
        g_timerActive = FALSE;
    }
    DeleteCriticalSection(&g_fileListLock);
}

BOOL HasPendingItems() {
    return g_enumInProgress;
}

// 加载布局状态
void loadLayoutState() {
    // 获取可执行文件目录
    WCHAR exePath[MAX_PATH];
    getExecutableDirectory(exePath, MAX_PATH);
    
    // 构造完整路径
    WCHAR filePath[MAX_PATH];
    lstrcpyW(filePath, exePath);
    lstrcatW(filePath, L"layout_state.ini");
    
    FILE* fp = NULL;
    errno_t err = _wfopen_s(&fp, filePath, L"r, ccs=UTF-8");
    if (err == 0 && fp) {
        WCHAR line[256];
        if (fgetws(line, 256, fp)) {
            if (wcsncmp(line, L"SplitterPos=", 12) == 0) {
                g_splitterPos = _wtoi(line + 12);
                if (g_splitterPos < 100) g_splitterPos = 100;
            }
        }
        fclose(fp);
    }
}

// 状态栏自定义绘制过程
LRESULT CALLBACK StatusBarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // 获取状态栏客户区大小
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            // 创建内存DC进行双缓冲绘制
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
            
            // 填充背景色
            HBRUSH bgBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
            FillRect(memDC, &rect, bgBrush);
            DeleteObject(bgBrush);
            
            // 绘制进度条区域（左侧）
            int progressWidth = 120; // 进度条固定宽度120像素
            int progressHeight = 12;
            int progressX = 5;
            int progressY = (rect.bottom - rect.top - progressHeight) / 2;
            
            RECT progressRect = {progressX, progressY, progressX + progressWidth, progressY + progressHeight};
            
            // 绘制进度条边框
            HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
            HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
            Rectangle(memDC, progressRect.left, progressRect.top, progressRect.right, progressRect.bottom);
            
            // 绘制进度条填充（根据磁盘占用比例）
            if (g_diskUsageRatio > 0.0) {
                int fillWidth = (int)((progressWidth - 2) * g_diskUsageRatio);
                RECT fillRect = {progressRect.left + 1, progressRect.top + 1, 
                                 progressRect.left + 1 + fillWidth, progressRect.bottom - 1};
                
                // 根据占用比例选择颜色
                COLORREF progressColor;
                if (g_diskUsageRatio < 0.5) {
                    progressColor = RGB(0, 200, 0); // 绿色
                } else if (g_diskUsageRatio < 0.8) {
                    progressColor = RGB(255, 165, 0); // 橙色
                } else {
                    progressColor = RGB(255, 0, 0); // 红色
                }
                
                HBRUSH fillBrush = CreateSolidBrush(progressColor);
                FillRect(memDC, &fillRect, fillBrush);
                DeleteObject(fillBrush);
            }
            
            SelectObject(memDC, oldPen);
            DeleteObject(borderPen);
            
            // 绘制磁盘占用百分比文本（进度条右侧）
            WCHAR percentText[64];
            int percent = (int)(g_diskUsageRatio * 100);
            swprintf_s(percentText, 64, L"磁盘占用: %d%%", percent);
            
            int percentTextX = progressRect.right + 10;
            RECT percentTextRect = {percentTextX, 0, percentTextX + 120, rect.bottom};
            
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, GetSysColor(COLOR_BTNTEXT));
            DrawTextW(memDC, percentText, -1, &percentTextRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            
            // 绘制磁盘空间信息文本（最右侧）
            if (g_diskSpaceInfo[0] != L'\0') {
                int spaceInfoX = percentTextRect.right + 20;
                RECT spaceInfoRect = {spaceInfoX, 0, rect.right, rect.bottom};
                
                DrawTextW(memDC, g_diskSpaceInfo, -1, &spaceInfoRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
            
            // 将内存DC内容复制到屏幕DC
            BitBlt(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, memDC, 0, 0, SRCCOPY);
            
            // 清理资源
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_ERASEBKGND:
            return 1; // 防止擦除背景，避免闪烁
    }
    
    // 调用原始窗口过程处理其他消息
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

#include <math.h>

// 绘制磁盘使用情况饼图
void DrawDiskPieChart(HDC hdc, RECT rect) {
    // 填充背景
    HBRUSH bgBrush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    // 仅在磁盘详情页显示
    if (wcscmp(g_currentPath, L"磁盘详情") != 0) return;

    EnterCriticalSection(&g_fileListLock);
    size_t count = g_fileList.size();
    if (count == 0) {
        LeaveCriticalSection(&g_fileListLock);
        return;
    }

    // 布局计算
    int availableWidth = rect.right - rect.left;
    int visibleHeight = rect.bottom - rect.top;
    
    // 如果没有足够空间，不绘制
    if (availableWidth < 100 || visibleHeight < 50) {
        LeaveCriticalSection(&g_fileListLock);
        return;
    }

    // 计算网格布局
    // 假设每个卡片宽度 300，高度 400 (包含饼图、条状图、文字)
    int cardWidth = 300;
    int cardHeight = 400;
    int padding = 20;
    
    // 计算可以放多少列
    int cols = (availableWidth - padding) / (cardWidth + padding);
    if (cols < 1) cols = 1;
    
    // 计算内容总宽度
    int totalContentWidth = cols * cardWidth + (cols - 1) * padding;
    
    // 计算总行数
    int rows = (int)((count + cols - 1) / cols);
    
    // 计算总内容高度
    int totalContentHeight = rows * (cardHeight + padding) + padding; // 底部留白
    
    // 更新滚动条信息
    SCROLLINFO si = {0};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = totalContentHeight;
    si.nPage = visibleHeight;
    si.nPos = g_diskScrollY;
    
    // 如果内容高度超过可视高度，显示滚动条
    if (totalContentHeight > visibleHeight) {
        // 确保滚动位置不越界
        if (g_diskScrollY > totalContentHeight - visibleHeight) {
             g_diskScrollY = totalContentHeight - visibleHeight;
             si.nPos = g_diskScrollY;
        }

        SetScrollInfo(g_mainWindow, SB_VERT, &si, TRUE);
        ShowScrollBar(g_mainWindow, SB_VERT, TRUE);
    } else {
        // 内容完全可见，隐藏滚动条
        ShowScrollBar(g_mainWindow, SB_VERT, FALSE);
        g_diskScrollY = 0; // 重置滚动位置
    }
    
    // 计算起始X坐标以居中
    int startX = rect.left + (availableWidth - totalContentWidth) / 2;
    if (startX < rect.left + padding) startX = rect.left + padding;
    
    int startY = rect.top + 20 - g_diskScrollY; // 应用滚动偏移

    // 通用绘图资源
    HBRUSH hFreeBrush = CreateSolidBrush(RGB(240, 240, 240)); // 浅灰背景
    HPEN hNullPen = CreatePen(PS_NULL, 0, 0);
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    HFONT hTitleFont = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
    HFONT hNormalFont = (HFONT)SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);

    for (size_t i = 0; i < count; ++i) {
        const ItemSortData& data = g_fileList[i];
        
        int col = i % cols;
        int row = i / cols;
        
        int x = startX + col * (cardWidth + padding);
        int y = startY + row * (cardHeight + padding);
        
        // 简单的可视区域剔除
        if (y + cardHeight < rect.top || y > rect.bottom) {
             continue; 
        }

        // 绘制卡片背景
        RECT cardRect = {x, y, x + cardWidth, y + cardHeight};
        FillRect(hdc, &cardRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        
        // 绘制卡片边框
        HPEN hOldPen = (HPEN)SelectObject(hdc, hBorderPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, cardRect.left, cardRect.top, cardRect.right, cardRect.bottom);
        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);

        if (data.totalSpace > 0) {
            double used = (double)(data.totalSpace - data.freeSpace);
            double ratio = used / (double)data.totalSpace;
            if (ratio < 0) ratio = 0;
            if (ratio > 1) ratio = 1;

            // 1. 标题 (盘符 + 卷标)
            RECT titleRect = {x + 10, y + 10, x + cardWidth - 10, y + 40};
            SelectObject(hdc, hTitleFont);
            DrawTextW(hdc, data.name.c_str(), -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(hdc, hNormalFont);

            // 2. 饼图区域
            int pieSize = 180;
            int pieX = x + (cardWidth - pieSize) / 2;
            int pieY = y + 50;
            int radius = pieSize / 2;
            int cx = pieX + radius;
            int cy = pieY + radius;

            // 画背景圆 (可用空间)
            hOldBrush = (HBRUSH)SelectObject(hdc, hFreeBrush);
            hOldPen = (HPEN)SelectObject(hdc, hNullPen);
            Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
            SelectObject(hdc, hOldBrush);

            // 画已用扇形
            if (ratio > 0.001) {
                COLORREF color;
                if (ratio < 0.75) color = RGB(0, 120, 215); // 蓝
                else if (ratio < 0.9) color = RGB(255, 165, 0); // 橙
                else color = RGB(220, 0, 0); // 红

                HBRUSH hUsedBrush = CreateSolidBrush(color);
                hOldBrush = (HBRUSH)SelectObject(hdc, hUsedBrush);

                int x1 = cx;
                int y1 = cy - radius; // 12点

                double angle = ratio * 2 * 3.1415926535;
                int x2 = cx + (int)(radius * sin(angle));
                int y2 = cy - (int)(radius * cos(angle));

                if (ratio >= 0.999) {
                    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
                } else {
                    Pie(hdc, cx - radius, cy - radius, cx + radius, cy + radius, x2, y2, x1, y1);
                }

                SelectObject(hdc, hOldBrush);
                DeleteObject(hUsedBrush);
            }
            SelectObject(hdc, hOldPen);

            // 3. 详细信息文字
            WCHAR szInfo[256];
            swprintf_s(szInfo, 256, L"已用: %.1f GB (%.1f%%)\n可用: %.1f GB\n总计: %.1f GB", 
                used / (1024.0*1024.0*1024.0),
                ratio * 100.0,
                (double)data.freeSpace / (1024.0*1024.0*1024.0),
                (double)data.totalSpace / (1024.0*1024.0*1024.0));
            
            RECT infoRect = {x + 20, pieY + pieSize + 10, x + cardWidth - 20, pieY + pieSize + 80};
            DrawTextW(hdc, szInfo, -1, &infoRect, DT_LEFT | DT_TOP);

            // 4. 条状图
            RECT barRect = {x + 20, infoRect.bottom + 10, x + cardWidth - 20, infoRect.bottom + 30};
            
            // 边框
            hOldPen = (HPEN)SelectObject(hdc, hBorderPen);
            hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, barRect.left, barRect.top, barRect.right, barRect.bottom);
            SelectObject(hdc, hOldBrush);
            SelectObject(hdc, hOldPen);
            
            // 填充
            InflateRect(&barRect, -1, -1);
            int barWidth = barRect.right - barRect.left;
            int fillWidth = (int)(barWidth * ratio);
            RECT fillRect = barRect;
            fillRect.right = fillRect.left + fillWidth;
            
            COLORREF barColor;
            if (ratio < 0.75) barColor = RGB(0, 120, 215);
            else if (ratio < 0.9) barColor = RGB(255, 165, 0);
            else barColor = RGB(220, 0, 0);
            
            HBRUSH hBarBrush = CreateSolidBrush(barColor);
            FillRect(hdc, &fillRect, hBarBrush);
            DeleteObject(hBarBrush);
        }
    }

    SelectObject(hdc, hNormalFont); // Restore font
    DeleteObject(hTitleFont);
    DeleteObject(hFreeBrush);
    DeleteObject(hNullPen);
    DeleteObject(hBorderPen);
    SetBkMode(hdc, oldBkMode);
    
    LeaveCriticalSection(&g_fileListLock);
}

// 窗口过程函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            loadLayoutState(); // 加载布局状态
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
            if (pt.x >= g_splitterPos && pt.x <= g_splitterPos + SPLITTER_WIDTH && pt.y > 65) {
                SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                return TRUE;
            }
            break; // 继续默认处理
        }
        
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            
            // 检查是否点击了分隔条 (在Y轴65像素以下)
            if (x >= g_splitterPos && x <= g_splitterPos + SPLITTER_WIDTH && y > 65) {
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

        case WM_MOUSEWHEEL: {
            if (wcscmp(g_currentPath, L"磁盘详情") == 0) {
                int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                // 向下滚动时 zDelta < 0，应该增加 ScrollPos
                int scrollAmount = -zDelta; 
                
                SCROLLINFO si = {0};
                si.cbSize = sizeof(SCROLLINFO);
                si.fMask = SIF_ALL;
                GetScrollInfo(hwnd, SB_VERT, &si);
                
                int oldPos = si.nPos;
                si.nPos += scrollAmount / 2; // 调整滚动速度
                
                // 边界检查
                if (si.nPos < si.nMin) si.nPos = si.nMin;
                if (si.nPos > si.nMax - (int)si.nPage) si.nPos = si.nMax - (int)si.nPage;
                
                if (si.nPos != oldPos) {
                    g_diskScrollY = si.nPos;
                    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            break;
        }

        case WM_VSCROLL: {
            if (wcscmp(g_currentPath, L"磁盘详情") == 0) {
                SCROLLINFO si = {0};
                si.cbSize = sizeof(SCROLLINFO);
                si.fMask = SIF_ALL;
                GetScrollInfo(hwnd, SB_VERT, &si);
                
                int oldPos = si.nPos;
                
                switch (LOWORD(wParam)) {
                    case SB_TOP: si.nPos = si.nMin; break;
                    case SB_BOTTOM: si.nPos = si.nMax; break;
                    case SB_LINEUP: si.nPos -= 20; break;
                    case SB_LINEDOWN: si.nPos += 20; break;
                    case SB_PAGEUP: si.nPos -= si.nPage; break;
                    case SB_PAGEDOWN: si.nPos += si.nPage; break;
                    case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
                }
                
                // 边界检查
                if (si.nPos < si.nMin) si.nPos = si.nMin;
                if (si.nPos > si.nMax - (int)si.nPage) si.nPos = si.nMax - (int)si.nPage;
                
                if (si.nPos != oldPos) {
                    g_diskScrollY = si.nPos;
                    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            break;
        }
        
        case WM_COMMAND: {
        if (HandleContextMenuManagerMessage(hwnd, WM_COMMAND, wParam, lParam)) {
            return 0;
        }

        int wmId = LOWORD(wParam);
            if (wmId == IDM_DEBUG) {
                HandleDebugCommand(hwnd, wParam);
            } else if (wmId == IDM_NEW_TAB) {
                AddNewTab(L"");
            } else if (wmId == IDM_CLOSE_TAB) {
                CloseTab(g_currentTabIndex);
            } else if (wmId == ID_BTN_NEW_TAB) {
                AddNewTab(L"");
            } else if (wmId == IDM_NEW_WINDOW) {
                HandleNewWindow();
            } else if (wmId == IDM_TILE_WINDOWS) {
                HandleTileWindows();
            } else if (wmId == IDM_NEW_DISK_DETAILS) {
                AddNewTab(L"磁盘详情");
            } else if (wmId == IDM_SET_DEFAULT) {
                if (SetAsDefaultFileManager()) {
                    MessageBoxW(hwnd, L"已成功设置为默认文件管理器！", L"成功", MB_OK | MB_ICONINFORMATION);
                    // 更新菜单勾选状态
                    HMENU hMenu = GetMenu(hwnd);
                    HMENU hToolsMenu = GetSubMenu(hMenu, 0); // 假设工具菜单是第一个
                    CheckMenuItem(hToolsMenu, IDM_SET_DEFAULT, MF_BYCOMMAND | MF_CHECKED);
                } else {
                    MessageBoxW(hwnd, L"设置失败，可能需要管理员权限或被安全软件拦截。", L"错误", MB_OK | MB_ICONERROR);
                }
            } else if (wmId == IDM_RESTORE_DEFAULT) {
                if (RestoreDefaultFileManager()) {
                    MessageBoxW(hwnd, L"已恢复系统默认设置。", L"成功", MB_OK | MB_ICONINFORMATION);
                    // 更新菜单勾选状态
                    HMENU hMenu = GetMenu(hwnd);
                    HMENU hToolsMenu = GetSubMenu(hMenu, 0);
                    CheckMenuItem(hToolsMenu, IDM_SET_DEFAULT, MF_BYCOMMAND | MF_UNCHECKED);
                } else {
                     MessageBoxW(hwnd, L"恢复失败，可能需要管理员权限。", L"错误", MB_OK | MB_ICONERROR);
                 }
             } else if (wmId == IDM_CONTEXT_MENU_MGR) {
                AddNewTab(L"右键菜单管理");
            }
             
             if ((HWND)lParam == g_goButton) {
                if (HIWORD(wParam) == BN_CLICKED) {
                    HandleGoButtonClick(hwnd);
                }
            } else if ((HWND)lParam == g_upButton && HIWORD(wParam) == BN_CLICKED) {
                HandleBackButtonClick(); // Reuse existing function which implements "Up" logic
            } else if ((HWND)lParam == g_openInExplorerButton && HIWORD(wParam) == BN_CLICKED) {
                ShellExecuteW(NULL, L"explore", g_currentPath, NULL, NULL, SW_SHOWNORMAL);
            } else if ((HWND)lParam == g_settingsButton && HIWORD(wParam) == BN_CLICKED) {
                ShowSettingsDialog(hwnd);
            } else if (LOWORD(wParam) == 1 || LOWORD(wParam) == 2 || LOWORD(wParam) == 3) {
                LogMessage(L"[DEBUG] WM_COMMAND 收到收藏夹命令: %d", LOWORD(wParam));
                HandleFavoriteCommands(wParam);
            }
            break;
        }
        
        case WM_NOTIFY: {
            if (HandleContextMenuManagerMessage(hwnd, WM_NOTIFY, wParam, lParam)) {
                return 0;
            }

            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (g_tabCtrl && pnmh->hwndFrom == g_tabCtrl) {
                if (pnmh->code == TCN_SELCHANGE) {
                    int index = TabCtrl_GetCurSel(g_tabCtrl);
                    SwitchToTab(index);
                    return 0;
                } else if (pnmh->code == NM_RCLICK) {
                    POINT pt;
                    GetCursorPos(&pt);
                    
                    // Hit test to see which tab
                    POINT clientPt = pt;
                    ScreenToClient(g_tabCtrl, &clientPt);
                    TCHITTESTINFO hti;
                    hti.pt = clientPt;
                    int tab = TabCtrl_HitTest(g_tabCtrl, &hti);
                    
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenuW(hMenu, MF_STRING, IDM_NEW_TAB, L"新建标签页");
                    
                    if (tab != -1) {
                          // Right clicked on a tab
                          SwitchToTab(tab); 
                          AppendMenuW(hMenu, MF_STRING, IDM_CLOSE_TAB, L"关闭标签页");
                     }
                    
                    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                    DestroyMenu(hMenu);
                    return 0;
                }
            }
            
            if (pnmh->hwndFrom == g_listView && pnmh->code == LVN_BEGINDRAG) {
                std::vector<std::wstring> draggedFiles;
                
                // Get selected items
                int iItem = -1;
                while ((iItem = ListView_GetNextItem(g_listView, iItem, LVNI_SELECTED)) != -1) {
                    EnterCriticalSection(&g_fileListLock);
                    if (iItem < (int)g_fileList.size()) {
                        std::wstring fullPath = g_currentPath;
                        if (fullPath.back() != L'\\') fullPath += L"\\";
                        fullPath += g_fileList[iItem].name;
                        draggedFiles.push_back(fullPath);
                    }
                    LeaveCriticalSection(&g_fileListLock);
                }
                
                if (!draggedFiles.empty()) {
                    CDataObject* pDataObject = new CDataObject(draggedFiles);
                    CDropSource* pDropSource = new CDropSource();
                    
                    DWORD dwEffect;
                    DoDragDrop(pDataObject, pDropSource, DROPEFFECT_COPY | DROPEFFECT_MOVE, &dwEffect);
                    
                    pDataObject->Release();
                    pDropSource->Release();
                    
                    // Note: If move operation occurred, updateFileList is needed.
                    // Even if copy, refreshing is safe.
                    // But if we moved to ourselves, we already refreshed in PerformFileOperation.
                    // If we moved to Explorer, Explorer handled it. We should check if files are gone?
                    // Simple approach: Always refresh.
                    updateFileList();
                }
                return 0;
            }

            // 处理通知消息
            return HandleNotificationMessages(hwnd, wParam, lParam);
        }
        break;

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            WCHAR dragPath[MAX_PATH];
            int count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
            std::vector<std::wstring> files;
            for (int i=0; i<count; i++) {
                if (DragQueryFileW(hDrop, i, dragPath, MAX_PATH)) {
                    files.push_back(dragPath);
                }
            }
            DragFinish(hDrop);
            BOOL isMove = (GetKeyState(VK_SHIFT) < 0);
            PerformFileOperation(files, g_currentPath, isMove);
            return 0;
        }

        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            if (wcscmp(g_currentPath, L"磁盘详情") != 0) {
                // 确保隐藏垂直滚动条
                ShowScrollBar(hwnd, SB_VERT, FALSE);

                // 绘制分隔线
                HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                
                int x = g_splitterPos + SPLITTER_WIDTH / 2;
                MoveToEx(hdc, x, 65, NULL);
                LineTo(hdc, x, rect.bottom);
                
                SelectObject(hdc, hOldPen);
                DeleteObject(hPen);
            } else {
                // 磁盘详情页绘制饼图
                // 使用完整的客户区，但要注意避开顶部的工具栏/地址栏 (假设 65px 高)
                RECT pieRect = rect;
                pieRect.top = 65; 
                DrawDiskPieChart(hdc, pieRect);
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_APP_UPDATE_COUNT: {
            if (!g_listView) return 0;
            EnterCriticalSection(&g_fileListLock);
            int count = (int)g_fileList.size();
            LeaveCriticalSection(&g_fileListLock);
            SendMessageW(g_listView, LVM_SETITEMCOUNT, (WPARAM)count, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
            return 0;
        }

        case WM_APP_SORTDONE: {
            if (!g_listView) return 0;
            InvalidateRect(g_listView, NULL, TRUE);
            UpdateWindow(g_listView);
            g_sorting = FALSE;
            return 0;
        }

        case WM_APP_DIRSIZE: {
            DirSizeResult* res = (DirSizeResult*)lParam;
            if (res) {
                if (g_sorting) { delete res; return 0; }
                if (lstrcmpiW(res->parent, g_currentPath) == 0) {
                     // 更新UI
                     UpdateListViewDirSize(res->parent, res->name, res->size, res->isPartial);
                }
                delete res;
            }
            return 0;
        }

        case WM_APP_LISTDONE: {
            g_enumInProgress = FALSE;
            EnterCriticalSection(&g_fileListLock);
            int count = (int)g_fileList.size();
            LeaveCriticalSection(&g_fileListLock);
            SendMessageW(g_listView, LVM_SETITEMCOUNT, (WPARAM)count, LVSICF_NOINVALIDATEALL);
            InvalidateRect(g_listView, NULL, TRUE);
            
            std::vector<std::wstring> dirs;
            EnterCriticalSection(&g_fileListLock);
            for (const auto& item : g_fileList) {
                if (item.isDir && item.name != L"." && item.name != L"..") {
                    dirs.push_back(item.name);
                }
            }
            LeaveCriticalSection(&g_fileListLock);
            
            if (!dirs.empty()) {
                DirSizeTask* task = new DirSizeTask();
                lstrcpyW(task->parent, g_currentPath);
                task->names = std::move(dirs);
                task->generation = g_dirSizeGen;
                CreateThread(NULL, 0, DirSizeWorker, task, 0, NULL);
            }
            return 0;
        }

        case WM_DESTROY: {
            HandleDestroyMessage(hwnd);
            // 保存收藏夹数据
            LogMessage(L"[DEBUG] 保存收藏夹数据到文件...");
            saveFavoritesToFile();
            LogMessage(L"[DEBUG] 收藏夹数据保存完成，共 %d 项", g_favoriteCount);

            // 保存树展开状态
            LogMessage(L"[DEBUG] 保存树展开状态...");
            saveTreeExpansionState();

            // 保存布局状态
            saveLayoutState();
            
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
    // 如果在磁盘详情页，禁止导航
    if (wcscmp(g_currentPath, L"磁盘详情") == 0) return;

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
        
        if (wcscmp(itemType, L"文件夹") == 0 || wcscmp(itemType, L"本地磁盘") == 0) {
            // 导航到子目录
            WCHAR newPath[MAX_PATH] = {0};
            if (wcscmp(itemType, L"本地磁盘") == 0) {
                // 如果是驱动器，直接使用名称部分（如 "C:"）
                // Item name is "C: (Label)" or just "C:"
                // We need to extract "C:"
                 WCHAR* spacePos = wcschr(itemName, L' ');
                 if (spacePos) {
                     wcsncpy_s(newPath, MAX_PATH, itemName, spacePos - itemName);
                 } else {
                     lstrcpyW(newPath, itemName);
                 }
                 lstrcatW(newPath, L"\\");
            } else {
                lstrcpyW(newPath, g_currentPath);
                if (newPath[lstrlenW(newPath) - 1] != L'\\') {
                    lstrcatW(newPath, L"\\");
                }
                lstrcatW(newPath, itemName);
            }
            
            // 检查路径是否存在且为目录
            DWORD attributes = GetFileAttributesW(newPath);
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                setCurrentDirectory(newPath);
                updateFileList();
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
        HMENU hToolsMenu = CreatePopupMenu();
        
        AppendMenuW(hToolsMenu, MF_STRING, IDM_NEW_WINDOW, L"新窗口");
        AppendMenuW(hToolsMenu, MF_STRING, IDM_TILE_WINDOWS, L"左右平铺窗口");
        AppendMenuW(hToolsMenu, MF_STRING, IDM_NEW_DISK_DETAILS, L"新建磁盘详情页");
        AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, NULL);
        
        // 检查是否已设置为默认，如果是，则勾选
        UINT flags = MF_STRING;
        if (IsDefaultFileManager()) {
            flags |= MF_CHECKED;
        }
        AppendMenuW(hToolsMenu, flags, IDM_SET_DEFAULT, L"设为默认文件管理器");
        AppendMenuW(hToolsMenu, MF_STRING, IDM_RESTORE_DEFAULT, L"恢复系统默认");
        AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hToolsMenu, MF_STRING, IDM_CONTEXT_MENU_MGR, L"右键菜单管理");
        
        AppendMenuW(hHelpMenu, MF_STRING, IDM_DEBUG, L"Debug");
        
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hToolsMenu, L"工具");
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, L"Help");
        SetMenu(hwnd, hMenu);

        // 验证窗口标题
        WCHAR windowTitle[256];
        GetWindowTextW(hwnd, windowTitle, 256);
        LogMessage(L"窗口标题: %s", windowTitle);
        
        DragAcceptFiles(hwnd, TRUE); // Enable Drop Files

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
            WCHAR targetPath[MAX_PATH];
            lstrcpyW(targetPath, favoriteItem->path);
            setCurrentDirectory(targetPath);

            // 自动展开目录树并跳转
            // 解析路径并展开树
            if (targetPath[0] != 0 && targetPath[1] == L':') {
                WCHAR drive[4] = {0};
                drive[0] = targetPath[0];
                drive[1] = targetPath[1];
                drive[2] = L'\\';
                drive[3] = L'\0';
                
                // 查找驱动器节点
                HTREEITEM hCurrent = findChildNode(g_treeView, NULL, drive);
                if (hCurrent) {
                    TreeView_Expand(g_treeView, hCurrent, TVE_EXPAND);
                    
                    // 复制路径用于分词
                    WCHAR pathCopy[MAX_PATH];
                    lstrcpyW(pathCopy, targetPath);
                    
                    // 跳过驱动器部分 (E:\)
                    WCHAR* start = wcschr(pathCopy, L'\\');
                    if (start && *(start + 1)) {
                        start++; // 指向第一个文件夹字符
                        
                        WCHAR* context = NULL;
                        WCHAR* nextComp = wcstok_s(start, L"\\", &context);
                        while (nextComp) {
                            HTREEITEM hChild = findChildNode(g_treeView, hCurrent, nextComp);
                            if (hChild) {
                                hCurrent = hChild;
                                TreeView_Expand(g_treeView, hCurrent, TVE_EXPAND);
                                nextComp = wcstok_s(NULL, L"\\", &context);
                            } else {
                                break;
                            }
                        }
                    }
                    
                    // 选中最终节点
                    TreeView_Select(g_treeView, hCurrent, TVGN_CARET);
                    TreeView_EnsureVisible(g_treeView, hCurrent);
                }
            }
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
    // 加载设置
    loadSettings();

    // 注册窗口类分配控制台窗口并输出日志
    LogMessage(L"资源管理器程序启动中...");
    
    
    // 初始化COM (使用OleInitialize以支持拖放)
    if (FAILED(OleInitialize(NULL))) {
        LogMessage(L"OLE初始化失败");
        return 1;
    }
    LogMessage(L"OLE初始化完成");
    
    // 注册窗口类
    if (!RegisterWindowClass(hInstance)) {
        MessageBoxW(NULL, L"窗口类注册失败", L"错误", MB_OK | MB_ICONERROR);
        LogMessage(L"窗口类注册失败");
        return 1;
    }
    LogMessage(L"窗口类注册成功");
    
    // 获取当前目录
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(pCmdLine, &argc);
    if (argv && argc > 0) {
        // Remove quotes if present is handled by CommandLineToArgvW
        // But argv[0] is the first argument, not program name in this case?
        // wWinMain pCmdLine contains only arguments.
        // CommandLineToArgvW parses a string. If pCmdLine is "C:\", argv[0] is C:\
        
        lstrcpyW(g_currentPath, argv[0]);
        LocalFree(argv);
    } else {
        lstrcpyW(g_currentPath, L"此电脑");
    }
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
    
    // 如果有初始路径，同步目录树选中状态
    if (wcslen(g_currentPath) > 0 && wcscmp(g_currentPath, L"此电脑") != 0 && wcscmp(g_currentPath, L"磁盘详情") != 0) {
        LogMessage(L"同步初始路径到目录树: %s", g_currentPath);
        syncTreeViewWithPath(g_currentPath);
    }
    
    LogMessage(L"资源管理器程序启动完成，进入消息循环");
    
    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    LogMessage(L"资源管理器程序退出");
    
    // 清理COM
    OleUninitialize();
    
    return 0;
}
void UpdateListViewDirSize(const WCHAR* parentPath, const WCHAR* name, ULONGLONG size, BOOL isPartial) {
    EnterCriticalSection(&g_fileListLock);
    int idx = -1;
    for (size_t i = 0; i < g_fileList.size(); ++i) {
        if (g_fileList[i].isDir && g_fileList[i].name == name) {
            g_fileList[i].sizeNumeric = size;
            g_fileList[i].isPartial = isPartial;
            idx = (int)i;
            break;
        }
    }
    LeaveCriticalSection(&g_fileListLock);

    if (idx != -1) {
        // 刷新特定项
        SendMessageW(g_listView, LVM_REDRAWITEMS, idx, idx);
        UpdateWindow(g_listView);
    }
}

DWORD WINAPI DirSizeWorker(LPVOID lpParam) {
    DirSizeTask* task = (DirSizeTask*)lpParam;
    if (!task) return 0;
    LONG myGen = task->generation;
    for (const auto& name : task->names) {
        if (myGen != g_dirSizeGen) break;
        WCHAR fullPath[MAX_PATH] = {0};
        lstrcpyW(fullPath, task->parent);
        if (fullPath[lstrlenW(fullPath) - 1] != L'\\') {
            lstrcatW(fullPath, L"\\");
        }
        lstrcatW(fullPath, name.c_str());

        ULONGLONG dirSize = 0;
        BOOL isComplete = FALSE;
        if (myGen != g_dirSizeGen) break;
        
        // 尝试从缓存获取
        if (getCachedDirSize(fullPath, &dirSize)) {
            isComplete = TRUE;
        } else {
            // 缓存未命中，计算大小
            dirSize = computeDirectorySize(fullPath, &isComplete);
            // 只有完整计算的结果才写入缓存
            if (isComplete) {
                setCachedDirSize(fullPath, dirSize);
            }
        }

        DirSizeResult* res = new DirSizeResult();
        lstrcpyW(res->parent, task->parent);
        lstrcpyW(res->name, name.c_str());
        res->size = dirSize;
        res->isPartial = !isComplete;
        PostMessageW(g_mainWindow, WM_APP_DIRSIZE, 0, (LPARAM)res);
        Sleep(1);
        // 注意：使用全局窗口句柄
    }
    delete task;
    return 0;
}

DWORD WINAPI ListEnumWorker(LPVOID lpParam) {
    ListEnumTask* t = (ListEnumTask*)lpParam;
    if (!t) return 0;
    LONG myGen = t->generation;
    if (myGen != g_dirSizeGen) { delete t; return 0; }
    
    WIN32_FIND_DATAW findData;
    WCHAR searchPath[MAX_PATH];
    lstrcpyW(searchPath, t->parent);
    int len = lstrlenW(searchPath);
    if (len > 0 && searchPath[len - 1] != L'\\') {
        lstrcatW(searchPath, L"\\");
    }
    lstrcatW(searchPath, L"*");
    
    std::vector<ItemSortData> chunk;
    chunk.reserve(1000);
    
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (myGen != g_dirSizeGen) break;
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) continue;
            
            ItemSortData r;
            r.name = findData.cFileName;
            r.isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            ULARGE_INTEGER fileSize;
            fileSize.LowPart = findData.nFileSizeLow;
            fileSize.HighPart = findData.nFileSizeHigh;
            r.sizeNumeric = fileSize.QuadPart;
            r.created = findData.ftCreationTime;
            r.modified = findData.ftLastWriteTime;
            r.isPartial = FALSE;
            
            chunk.push_back(r);
            
            if (chunk.size() >= 500) {
                EnterCriticalSection(&g_fileListLock);
                if (myGen == g_dirSizeGen) {
                    g_fileList.insert(g_fileList.end(), chunk.begin(), chunk.end());
                }
                LeaveCriticalSection(&g_fileListLock);
                chunk.clear();
                
                if (myGen == g_dirSizeGen) {
                    PostMessageW(g_mainWindow, WM_APP_UPDATE_COUNT, 0, 0);
                }
                Sleep(1);
            }
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
    
    if (!chunk.empty() && myGen == g_dirSizeGen) {
        EnterCriticalSection(&g_fileListLock);
        g_fileList.insert(g_fileList.end(), chunk.begin(), chunk.end());
        LeaveCriticalSection(&g_fileListLock);
        PostMessageW(g_mainWindow, WM_APP_UPDATE_COUNT, 0, 0);
    }
    
    if (myGen == g_dirSizeGen) {
        PostMessageW(g_mainWindow, WM_APP_LISTDONE, 0, 0);
    }
    delete t;
    return 0;
}
