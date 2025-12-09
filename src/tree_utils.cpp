#include "tree_utils.h"
#include "favorites.h"
#include "log.h"
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>

// 声明全局变量（这些应该在其他地方定义）
extern HWND g_treeView;
extern HTREEITEM g_favoritesNode;
extern WCHAR g_currentPath[MAX_PATH];

// 获取逻辑驱动器
void getLogicalDrives(WCHAR drives[][4], int* count) {
    *count = 0;
    DWORD driveMask = GetLogicalDrives();
    
    for (int i = 0; i < 26; i++) {
        if (driveMask & (1 << i)) {
            swprintf_s(drives[*count], 4, L"%c:\\", L'A' + i);
            (*count)++;
        }
    }
}

// 检查目录是否有子目录
BOOL hasSubdirectories(const WCHAR* path) {
    WIN32_FIND_DATAW findData;
    WCHAR searchPath[MAX_PATH];
    
    lstrcpyW(searchPath, path);
    lstrcatW(searchPath, L"\\*");
    
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    do {
        // 跳过.和..
        if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                FindClose(hFind);
                return TRUE;
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    return FALSE;
}

// 获取节点完整路径
void getNodeFullPath(HWND treeView, HTREEITEM hItem, WCHAR* fullPath, int bufferSize) {
    WCHAR pathParts[100][MAX_PATH] = {0};
    int partCount = 0;
    
    // 从当前节点向上遍历到根节点，收集路径部分
    HTREEITEM currentItem = hItem;
    while (currentItem) {
        TVITEMW tvi = {0};
        tvi.hItem = currentItem;
        tvi.mask = TVIF_TEXT;
        tvi.pszText = pathParts[partCount];
        tvi.cchTextMax = MAX_PATH;
        
        if (!TreeView_GetItem(treeView, &tvi)) {
            break;
        }
        
        partCount++;
        currentItem = TreeView_GetParent(treeView, currentItem);
    }
    
    // 构建完整路径（从根到叶）
    fullPath[0] = L'\0';
    for (int i = partCount - 1; i >= 0; i--) {
        if (i < partCount - 1) {
            lstrcatW(fullPath, L"\\");
        }
        lstrcatW(fullPath, pathParts[i]);
    }
}

// 添加占位符节点
void addPlaceholderNode(HWND treeView, HTREEITEM parent) {
    TVINSERTSTRUCTW tvis = {0};
    tvis.hParent = parent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvis.item.pszText = (LPWSTR)L"(正在加载...)";
    tvis.item.lParam = 0;  // 占位符节点
    
    TreeView_InsertItem(treeView, &tvis);
}

// 展开目录节点
void expandDirectoryNode(HWND treeView, HTREEITEM hItem, const WCHAR* path) {
    LogMessage(L"[DEBUG] expandDirectoryNode 开始: %s", path);
    
    // 删除现有的子节点
    HTREEITEM hChild = TreeView_GetChild(treeView, hItem);
    while (hChild) {
        HTREEITEM hNext = TreeView_GetNextSibling(treeView, hChild);
        TreeView_DeleteItem(treeView, hChild);
        hChild = hNext;
    }
    
    // 添加新的子目录
    WIN32_FIND_DATAW findData;
    WCHAR searchPath[MAX_PATH];
    
    lstrcpyW(searchPath, path);
    lstrcatW(searchPath, L"\\*");
    
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            // 只处理目录
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                wcscmp(findData.cFileName, L".") != 0 &&
                wcscmp(findData.cFileName, L"..") != 0) {
                
                // 创建树节点
                TVINSERTSTRUCTW tvis = {0};
                tvis.hParent = hItem;
                tvis.hInsertAfter = TVI_LAST;
                tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
                tvis.item.pszText = findData.cFileName;
                tvis.item.lParam = 0;  // 普通目录节点
                
                HTREEITEM hNewItem = TreeView_InsertItem(treeView, &tvis);
                if (hNewItem) {
                    // 检查该子目录是否有自己的子目录
                    WCHAR subPath[MAX_PATH];
                    lstrcpyW(subPath, path);
                    lstrcatW(subPath, L"\\");
                    lstrcatW(subPath, findData.cFileName);
                    
                    if (hasSubdirectories(subPath)) {
                        // 添加占位符节点以表示有子目录
                        addPlaceholderNode(treeView, hNewItem);
                    }
                }
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
    }
    
    LogMessage(L"[DEBUG] expandDirectoryNode 完成");
}

// 更新目录树
void updateDirectoryTree() {
    LogMessage(L"[DEBUG] updateDirectoryTree 开始");
    
    if (!g_treeView) {
        LogMessage(L"[DEBUG] g_treeView 为空，跳过更新");
        return;
    }
    
    // 清空现有树
    TreeView_DeleteAllItems(g_treeView);
    
    // 添加收藏夹节点
    if (g_favoritesNode) {
        TreeView_DeleteItem(g_treeView, g_favoritesNode);
    }
    g_favoritesNode = createFavoritesNode();
    
    // 加载收藏夹项到树中
    loadFavoritesIntoTree();
    
    // 获取逻辑驱动器
    WCHAR drives[26][4];
    int driveCount;
    getLogicalDrives(drives, &driveCount);
    
    // 为每个驱动器创建节点
    for (int i = 0; i < driveCount; i++) {
        TVINSERTSTRUCTW tvis = {0};
        tvis.hParent = NULL;
        tvis.hInsertAfter = TVI_ROOT;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = drives[i];
        tvis.item.lParam = 0;  // 普通驱动器节点
        
        HTREEITEM hDriveItem = TreeView_InsertItem(g_treeView, &tvis);
        if (hDriveItem) {
            // 为驱动器添加占位符节点
            addPlaceholderNode(g_treeView, hDriveItem);
        }
    }
    
    LogMessage(L"[DEBUG] updateDirectoryTree 完成");
}

// 处理树节点展开
void handleTreeItemExpanding(HWND treeView, HTREEITEM hItem) {
    // 获取节点文本（即目录名）
    WCHAR itemText[MAX_PATH] = {0};
    TVITEMW tvItem = {0};
    tvItem.hItem = hItem;
    tvItem.mask = TVIF_TEXT;
    tvItem.pszText = itemText;
    tvItem.cchTextMax = MAX_PATH;
    
    if (SendMessageW(treeView, TVM_GETITEMW, 0, (LPARAM)&tvItem)) {
        // 构造完整路径
        WCHAR fullPath[MAX_PATH] = {0};
        getNodeFullPath(treeView, hItem, fullPath, MAX_PATH);
        
        LogMessage(L"[DEBUG] 展开目录节点: %s", fullPath);
        expandDirectoryNode(treeView, hItem, fullPath);
    }
}