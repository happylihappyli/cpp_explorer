#include "tree_utils.h"
#include "favorites.h"
#include "log.h"
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <sstream>

using namespace std;

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
    int len = lstrlenW(searchPath);
    if (len > 0 && searchPath[len - 1] != L'\\') {
        lstrcatW(searchPath, L"\\");
    }
    lstrcatW(searchPath, L"*");
    
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
            int len = lstrlenW(fullPath);
            if (len > 0 && fullPath[len - 1] != L'\\') {
                lstrcatW(fullPath, L"\\");
            }
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
    int len = lstrlenW(searchPath);
    if (len > 0 && searchPath[len - 1] != L'\\') {
        lstrcatW(searchPath, L"\\");
    }
    lstrcatW(searchPath, L"*");
    
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
                    int subLen = lstrlenW(subPath);
                    if (subLen > 0 && subPath[subLen - 1] != L'\\') {
                        lstrcatW(subPath, L"\\");
                    }
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

// 辅助函数：递归保存展开的节点
void saveExpandedNodes(HWND treeView, HTREEITEM hItem, FILE* fp) {
    if (!hItem) return;

    do {
        // 检查节点状态
        UINT state = TreeView_GetItemState(treeView, hItem, TVIF_STATE);
        if (state & TVIS_EXPANDED) {
            WCHAR fullPath[MAX_PATH] = {0};
            getNodeFullPath(treeView, hItem, fullPath, MAX_PATH);
            
            if (lstrlenW(fullPath) > 0) {
                fwprintf(fp, L"%s\n", fullPath);
                
                // 递归处理子节点
                HTREEITEM hChild = TreeView_GetChild(treeView, hItem);
                if (hChild) {
                    saveExpandedNodes(treeView, hChild, fp);
                }
            }
        }
        
        hItem = TreeView_GetNextSibling(treeView, hItem);
    } while (hItem);
}

#include "file_utils.h"

// 保存树展开状态
void saveTreeExpansionState() {
    if (!g_treeView) return;
    
    // 获取可执行文件目录
    WCHAR exePath[MAX_PATH];
    getExecutableDirectory(exePath, MAX_PATH);
    
    // 构造完整路径
    WCHAR filePath[MAX_PATH];
    lstrcpyW(filePath, exePath);
    lstrcatW(filePath, L"tree_state.txt");
    
    FILE* fp = NULL;
    errno_t err = _wfopen_s(&fp, filePath, L"w, ccs=UTF-8");
    if (err != 0 || !fp) {
        LogMessage(L"[ERROR] 无法打开 %s 进行写入", filePath);
        return;
    }
    
    LogMessage(L"[DEBUG] 开始保存树展开状态到: %s", filePath);
    
    // 从根节点开始遍历
    HTREEITEM hRoot = TreeView_GetRoot(g_treeView);
    saveExpandedNodes(g_treeView, hRoot, fp);
    
    fclose(fp);
    LogMessage(L"[DEBUG] 树展开状态保存完成");
}

// 辅助函数：查找子节点
HTREEITEM findChildNode(HWND treeView, HTREEITEM hParent, const WCHAR* name) {
    HTREEITEM hChild = hParent ? TreeView_GetChild(treeView, hParent) : TreeView_GetRoot(treeView);
    while (hChild) {
        WCHAR text[MAX_PATH] = {0};
        TVITEMW tvi = {0};
        tvi.hItem = hChild;
        tvi.mask = TVIF_TEXT;
        tvi.pszText = text;
        tvi.cchTextMax = MAX_PATH;
        TreeView_GetItem(treeView, &tvi);
        
        if (lstrcmpiW(text, name) == 0) {
            return hChild;
        }
        hChild = TreeView_GetNextSibling(treeView, hChild);
    }
    return NULL;
}

// 恢复树展开状态
void restoreTreeExpansionState() {
    if (!g_treeView) return;
    
    // 获取可执行文件目录
    WCHAR exePath[MAX_PATH];
    getExecutableDirectory(exePath, MAX_PATH);
    
    // 构造完整路径
    WCHAR filePath[MAX_PATH];
    lstrcpyW(filePath, exePath);
    lstrcatW(filePath, L"tree_state.txt");
    
    FILE* fp = NULL;
    errno_t err = _wfopen_s(&fp, filePath, L"r, ccs=UTF-8");
    if (err != 0 || !fp) {
        // 文件可能不存在，这是正常的
        LogMessage(L"[INFO] 未找到树状态文件: %s", filePath);
        return;
    }
    
    LogMessage(L"[DEBUG] 开始从 %s 恢复树展开状态...", filePath);
    
    WCHAR line[MAX_PATH];
    while (fgetws(line, MAX_PATH, fp)) {
        // 去除换行符
        int len = lstrlenW(line);
        while (len > 0 && (line[len-1] == L'\n' || line[len-1] == L'\r')) {
            line[len-1] = L'\0';
            len--;
        }
        
        if (len == 0) continue;
        LogMessage(L"[DEBUG] 读取到路径: %s", line);
        
        // 简单的路径解析和展开
        // 我们使用一个副本进行分割
        WCHAR pathCopy[MAX_PATH];
        lstrcpyW(pathCopy, line);
        
        WCHAR* context = NULL;
        WCHAR* token = wcstok_s(pathCopy, L"\\", &context);
        
        HTREEITEM hCurrent = NULL;
        
        if (token) {
            LogMessage(L"[DEBUG] 解析根节点: %s", token);
            // 尝试直接查找根节点
            hCurrent = findChildNode(g_treeView, NULL, token);
            
            if (!hCurrent) {
                // 如果找不到，尝试加上反斜杠（针对驱动器节点）
                WCHAR tokenWithSlash[MAX_PATH];
                lstrcpyW(tokenWithSlash, token);
                lstrcatW(tokenWithSlash, L"\\");
                LogMessage(L"[DEBUG] 尝试查找带反斜杠的根节点: %s", tokenWithSlash);
                hCurrent = findChildNode(g_treeView, NULL, tokenWithSlash);
            }
            
            if (hCurrent) {
                LogMessage(L"[DEBUG] 找到根节点，展开之");
                // 展开根节点
                TreeView_Expand(g_treeView, hCurrent, TVE_EXPAND);
                
                // 继续处理后续路径部分
                while ((token = wcstok_s(NULL, L"\\", &context)) != NULL) {
                    LogMessage(L"[DEBUG] 查找子节点: %s", token);
                    // 查找子节点
                    HTREEITEM hNext = findChildNode(g_treeView, hCurrent, token);
                    if (hNext) {
                        hCurrent = hNext;
                        TreeView_Expand(g_treeView, hCurrent, TVE_EXPAND);
                    } else {
                        LogMessage(L"[DEBUG] 未找到子节点: %s", token);
                        break; // 找不到路径部分，停止
                    }
                }
            } else {
                LogMessage(L"[DEBUG] 未找到根节点: %s", token);
            }
        }
    }
    
    fclose(fp);
    LogMessage(L"[DEBUG] 树展开状态恢复完成");
}