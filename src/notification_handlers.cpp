#include "notification_handlers.h"
#include "favorites.h"
#include "tree_utils.h"
#include "file_utils.h"
#include "settings.h"
#include <string>
#include <vector>
#include <algorithm>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <wchar.h>
#include "log.h"
#define IDT_UI_BATCH 100
#define WM_APP_SORTDONE (WM_APP + 5)

// 外部变量声明
extern HWND g_listView;
extern HWND g_treeView;
extern HWND g_mainWindow;
extern HWND g_addressBar;
extern HTREEITEM g_favoritesNode;
extern WCHAR g_currentPath[MAX_PATH];
extern BOOL g_timerActive;
extern BOOL g_sorting;
extern BOOL HasPendingItems();
extern std::vector<ItemSortData> g_fileList;
extern CRITICAL_SECTION g_fileListLock;

// 函数声明
void HandleListViewDoubleClick(HWND hwnd, LPARAM lParam);
void HandleTreeViewDoubleClick(HWND hwnd, HWND mainWindow);
void setCurrentDirectory(const WCHAR* path);
void updateFileList();
void loadFavoritesIntoTree();

// 创建快捷方式
HRESULT CreateLink(LPCWSTR lpszPathObj, LPCWSTR lpszPathLink, LPCWSTR lpszDesc)
{
    HRESULT hres;
    IShellLink* psl;

    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hres))
    {
        IPersistFile* ppf;

        psl->SetPath(lpszPathObj);
        psl->SetDescription(lpszDesc);

        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
        if (SUCCEEDED(hres))
        {
            hres = ppf->Save(lpszPathLink, TRUE);
            ppf->Release();
        }
        psl->Release();
    }
    return hres;
}


struct SortParam { int column; BOOL asc; };

bool CompareItems(const ItemSortData& a, const ItemSortData& b, int column, BOOL asc) {
    int res = 0;
    switch (column) {
        case 0: res = _wcsicmp(a.name.c_str(), b.name.c_str()); break;
        case 1: 
            if (a.sizeNumeric == b.sizeNumeric) res = 0;
            else res = (a.sizeNumeric > b.sizeNumeric ? 1 : -1);
            break;
        case 2:
            {
                int ta = a.isDir ? 0 : 1;
                int tb = b.isDir ? 0 : 1;
                res = (ta < tb ? -1 : (ta > tb ? 1 : 0));
            }
            break;
        case 3:
            {
                ULONGLONG va = (((ULONGLONG)a.modified.dwHighDateTime) << 32) | a.modified.dwLowDateTime;
                ULONGLONG vb = (((ULONGLONG)b.modified.dwHighDateTime) << 32) | b.modified.dwLowDateTime;
                res = (va < vb ? -1 : (va > vb ? 1 : 0));
            }
            break;
        case 4:
            {
                ULONGLONG va = (((ULONGLONG)a.created.dwHighDateTime) << 32) | a.created.dwLowDateTime;
                ULONGLONG vb = (((ULONGLONG)b.created.dwHighDateTime) << 32) | b.created.dwLowDateTime;
                res = (va < vb ? -1 : (va > vb ? 1 : 0));
            }
            break;
    }
    if (!asc) res = -res;
    return res < 0;
}

DWORD WINAPI SortWorker(LPVOID lpParam) {
    SortParam* sp = (SortParam*)lpParam;
    if (!sp) return 0;
    
    EnterCriticalSection(&g_fileListLock);
    std::sort(g_fileList.begin(), g_fileList.end(), [sp](const ItemSortData& a, const ItemSortData& b) {
        return CompareItems(a, b, sp->column, sp->asc);
    });
    LeaveCriticalSection(&g_fileListLock);
    
    delete sp;
    PostMessageW(g_mainWindow, WM_APP_SORTDONE, 0, 0);
    return 0;
}

LRESULT HandleNotificationMessages(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    LPNMHDR nmhdr = (LPNMHDR)lParam;
    
    switch (nmhdr->code) {
        case NM_DBLCLK: {
            // ListView双击事件
            if (nmhdr->hwndFrom == g_listView) {
                HandleListViewDoubleClick(hwnd, lParam);
            }
            // TreeView双击事件
            else if (nmhdr->hwndFrom == g_treeView) {
                HandleTreeViewDoubleClick(hwnd, g_mainWindow);
            }
            break;
        }
        
        case NM_RCLICK: {
            // 处理右键点击事件
            POINT pt;
            GetCursorPos(&pt);

            // 处理ListView右键点击
            if (nmhdr->hwndFrom == g_listView) {
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
                    
                    // 构造完整路径
                    WCHAR fullPath[MAX_PATH];
                    lstrcpyW(fullPath, g_currentPath);
                    if (fullPath[lstrlenW(fullPath) - 1] != L'\\') {
                        lstrcatW(fullPath, L"\\");
                    }
                    lstrcatW(fullPath, itemName);

                    // 创建弹出菜单
                    HMENU hPopupMenu = CreatePopupMenu();
                    
                    // 通用选项：删除到回收站
                    AppendMenuW(hPopupMenu, MF_STRING, 203, L"删除到回收站");

                    // 检查是否为目录
                    DWORD attrs = GetFileAttributesW(fullPath);
                    BOOL isDir = (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY));

                    if (!isDir) {
                        WCHAR editorPath[MAX_PATH];
                        getEditorPath(editorPath, MAX_PATH);
                        WCHAR* editorName = PathFindFileNameW(editorPath);
                        WCHAR menuText[256];
                        wsprintfW(menuText, L"用 %s 打开", editorName);
                        AppendMenuW(hPopupMenu, MF_STRING, 204, menuText);
                    }

                    // 获取扩展名
                    WCHAR* ext = PathFindExtensionW(itemName);

                    // 动态添加文件关联菜单项
                    std::vector<FileAssociation> assocs = getFileAssociations();
                    std::vector<std::wstring> matchedCommands;
                    int baseCmdId = 300;
                    
                    if (!isDir) {
                        for (const auto& assoc : assocs) {
                            if (lstrcmpiW(assoc.ext.c_str(), ext) == 0) {
                                WCHAR menuText[256];
                                wsprintfW(menuText, L"用 %s 打开", assoc.name.c_str());
                                AppendMenuW(hPopupMenu, MF_STRING, baseCmdId + (int)matchedCommands.size(), menuText);
                                matchedCommands.push_back(assoc.command);
                            }
                        }
                    }

                    if (lstrcmpiW(ext, L".exe") == 0) {
                        AppendMenuW(hPopupMenu, MF_SEPARATOR, 0, NULL);
                        AppendMenuW(hPopupMenu, MF_STRING, 201, L"固定到开始菜单");
                        AppendMenuW(hPopupMenu, MF_STRING, 202, L"取消固定到开始菜单");
                    }
                    
                    // 显示菜单并获取选择的命令ID
                    int cmd = TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, NULL);
                    DestroyMenu(hPopupMenu);
                    
                    if (cmd == 203) { // 删除到回收站
                        if (DeleteToRecycleBin(fullPath)) {
                            // 刷新列表
                            updateFileList();
                        }
                    } else if (cmd == 204) { // 用编辑器打开
                        WCHAR editorPath[MAX_PATH];
                        getEditorPath(editorPath, MAX_PATH);
                        ShellExecuteW(NULL, L"open", editorPath, fullPath, NULL, SW_SHOWNORMAL);
                    } else if (cmd >= 300 && cmd < 300 + (int)matchedCommands.size()) {
                        std::wstring command = matchedCommands[cmd - 300];
                        ShellExecuteW(NULL, L"open", command.c_str(), fullPath, NULL, SW_SHOWNORMAL);
                    } else if (lstrcmpiW(ext, L".exe") == 0 && (cmd == 201 || cmd == 202)) {
                        // 获取开始菜单程序目录
                        WCHAR startMenuPath[MAX_PATH];
                        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, startMenuPath))) {
                            // 构造快捷方式路径
                            WCHAR linkPath[MAX_PATH];
                            lstrcpyW(linkPath, startMenuPath);
                            lstrcatW(linkPath, L"\\");
                            
                            // 获取不带扩展名的文件名
                            WCHAR nameWithoutExt[MAX_PATH];
                            lstrcpyW(nameWithoutExt, itemName);
                            PathRemoveExtensionW(nameWithoutExt);
                            
                            lstrcatW(linkPath, nameWithoutExt);
                            lstrcatW(linkPath, L".lnk");
                            
                            if (cmd == 201) { // 固定到开始菜单
                                if (SUCCEEDED(CreateLink(fullPath, linkPath, L"Pinned by MyExplorer"))) {
                                    MessageBoxW(hwnd, L"已固定到开始菜单", L"成功", MB_OK | MB_ICONINFORMATION);
                                } else {
                                    MessageBoxW(hwnd, L"固定到开始菜单失败", L"错误", MB_OK | MB_ICONERROR);
                                }
                            } else if (cmd == 202) { // 取消固定
                                if (DeleteFileW(linkPath)) {
                                    MessageBoxW(hwnd, L"已取消固定", L"成功", MB_OK | MB_ICONINFORMATION);
                                } else {
                                    MessageBoxW(hwnd, L"取消固定失败或未找到快捷方式", L"错误", MB_OK | MB_ICONERROR);
                                }
                            }
                        }
                    }
                }
                break;
            }
            
            // 处理TreeView右键点击
            // 获取点击位置的项
            TVHITTESTINFO ht = {};  // 修复编译器警告
            ht.pt = pt;
            ScreenToClient(g_treeView, &ht.pt);
            HTREEITEM hClickedItem = TreeView_HitTest(g_treeView, &ht);
            
            if (hClickedItem) {
                // 选中该项
                TreeView_SelectItem(g_treeView, hClickedItem);
                
                // 获取项的数据
                TVITEMW tvItem = {0};
                tvItem.hItem = hClickedItem;
                tvItem.mask = TVIF_PARAM;
                TreeView_GetItem(g_treeView, &tvItem);
                
                // 创建弹出菜单
                HMENU hPopupMenu = CreatePopupMenu();
                
                // 检查是否为收藏夹项（不是收藏夹根节点）
                if (tvItem.lParam != 0 && tvItem.lParam != (LPARAM)FAVORITE_ITEM_MARKER) {
                    // 检查是否为收藏夹根节点
                    if (hClickedItem == g_favoritesNode) {
                        // 点击的是收藏夹根节点，显示添加收藏项
                        AppendMenuW(hPopupMenu, MF_STRING, 2, L"添加当前路径到收藏夹");
                    } else {
                        // 检查是否为收藏夹项
                        FavoriteItem* favoriteItem = (FavoriteItem*)tvItem.lParam;
                        if (favoriteItem >= g_favorites && favoriteItem < g_favorites + g_favoriteCount) {
                            // 点击的是收藏夹项，显示编辑和移除选项
                            AppendMenuW(hPopupMenu, MF_STRING, 3, L"编辑名称");
                            AppendMenuW(hPopupMenu, MF_STRING, 1, L"移除此收藏项");
                        } else {
                            // 点击的是普通目录项，显示添加收藏项
                            AppendMenuW(hPopupMenu, MF_STRING, 2, L"添加当前路径到收藏夹");
                        }
                    }
                } else {
                    // 点击的是普通目录项，显示添加收藏项
                    AppendMenuW(hPopupMenu, MF_STRING, 2, L"添加当前路径到收藏夹");
                }
                
                // 显示菜单
                TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hPopupMenu);
            }
            break;
        }
        
        case TVN_ITEMEXPANDINGW: {
            LPNMTREEVIEWW pnmtv = (LPNMTREEVIEWW)lParam;
            if (pnmtv->action == TVE_EXPAND) {
                LogMessage(L"[DEBUG] TVN_ITEMEXPANDINGW: 展开节点");
                
                // 检查是否为收藏夹节点
                if (pnmtv->itemNew.hItem == g_favoritesNode) {
                    // 展开收藏夹节点，加载所有收藏项
                    loadFavoritesIntoTree();
                    // 展开后需要刷新UI
                    InvalidateRect(g_treeView, NULL, TRUE);
                    UpdateWindow(g_treeView);
                } else {
                    // 检查是否为收藏夹项（具有有效lParam的项）
                    TVITEMW tvItem = {0};
                    WCHAR itemText[MAX_PATH] = {0};
                    tvItem.hItem = pnmtv->itemNew.hItem;
                    tvItem.mask = TVIF_TEXT | TVIF_PARAM;
                    tvItem.pszText = itemText;
                    tvItem.cchTextMax = MAX_PATH;
                    
                    if (SendMessageW(g_treeView, TVM_GETITEMW, 0, (LPARAM)&tvItem)) {
                        // 如果lParam指向一个收藏夹项，则这是收藏夹中的目录
                        // 修改这里：检查lParam是否指向收藏夹数组中的项
                        BOOL isFavoriteItem = FALSE;
                        FavoriteItem* pFavoriteItem = NULL;
                        for (int i = 0; i < g_favoriteCount; i++) {
                            if (tvItem.lParam == (LPARAM)&g_favorites[i]) {
                                isFavoriteItem = TRUE;
                                pFavoriteItem = &g_favorites[i];
                                break;
                            }
                        }
                        
                        if (isFavoriteItem && pFavoriteItem != NULL) {
                            // 这是一个收藏夹中的目录项，使用收藏夹项中存储的完整路径
                            LogMessage(L"[DEBUG] 展开收藏夹目录: %s", pFavoriteItem->path);
                            expandDirectoryNode(g_treeView, pnmtv->itemNew.hItem, pFavoriteItem->path);
                            // 展开后需要刷新UI
                            InvalidateRect(g_treeView, NULL, TRUE);
                            UpdateWindow(g_treeView);
                        } else {
                            // 普通目录节点
                            handleTreeItemExpanding(g_treeView, pnmtv->itemNew.hItem);
                        }
                    } else {
                        // 获取节点信息失败，按普通节点处理
                        handleTreeItemExpanding(g_treeView, pnmtv->itemNew.hItem);
                    }
                }
            } else if (pnmtv->action == TVE_COLLAPSE) {
                LogMessage(L"[DEBUG] TVN_ITEMEXPANDINGW: 折叠节点");
                
                // 如果折叠的是收藏夹项，我们需要重新加载收藏夹树
                TVITEMW tvItem = {0};
                WCHAR itemText[MAX_PATH] = {0};
                tvItem.hItem = pnmtv->itemNew.hItem;
                tvItem.mask = TVIF_TEXT | TVIF_PARAM;
                tvItem.pszText = itemText;
                tvItem.cchTextMax = MAX_PATH;
                
                if (SendMessageW(g_treeView, TVM_GETITEMW, 0, (LPARAM)&tvItem)) {
                    // 检查是否为收藏夹项
                    BOOL isFavoriteItem = FALSE;
                    FavoriteItem* pFavoriteItem = NULL;
                    for (int i = 0; i < g_favoriteCount; i++) {
                        if (tvItem.lParam == (LPARAM)&g_favorites[i]) {
                            isFavoriteItem = TRUE;
                            pFavoriteItem = &g_favorites[i];
                            break;
                        }
                    }
                    
                    if (isFavoriteItem && pFavoriteItem != NULL) {
                        // 折叠收藏夹项时，删除所有子节点并添加占位符
                        HTREEITEM hChild = TreeView_GetChild(g_treeView, pnmtv->itemNew.hItem);
                        while (hChild) {
                            HTREEITEM hNext = TreeView_GetNextSibling(g_treeView, hChild);
                            TreeView_DeleteItem(g_treeView, hChild);
                            hChild = hNext;
                        }
                        // 添加占位符节点
                        addPlaceholderNode(g_treeView, pnmtv->itemNew.hItem);
                    }
                }
            }
            return 0;  // 允许展开/折叠操作
        }
        
        case TVN_SELCHANGEDW: {
            LPNMTREEVIEWW pnmtv = (LPNMTREEVIEWW)lParam;
            HTREEITEM hItem = pnmtv->itemNew.hItem;
            if (hItem) {
                // 获取项的数据
                TVITEMW tvi = {0};
                tvi.hItem = hItem;
                tvi.mask = TVIF_PARAM;
                TreeView_GetItem(g_treeView, &tvi);
                
                // 检查是否为收藏夹根节点
                if (tvi.lParam == FAVORITE_ITEM_MARKER) {
                    // 设置特殊路径标识
                    lstrcpyW(g_currentPath, L"★ 收藏夹");
                    LogMessage(L"[DEBUG] 选中收藏夹根节点");
                    updateFileList();
                    break;
                } 
                
                // 检查是否为收藏夹项
                BOOL isFavoriteItem = FALSE;
                FavoriteItem* pFavoriteItem = NULL;
                for (int i = 0; i < g_favoriteCount; i++) {
                    if (tvi.lParam == (LPARAM)&g_favorites[i]) {
                        isFavoriteItem = TRUE;
                        pFavoriteItem = &g_favorites[i];
                        break;
                    }
                }
                
                if (isFavoriteItem && pFavoriteItem != NULL) {
                    // 这是一个收藏夹项，直接跳转到对应的路径
                    LogMessage(L"[DEBUG] 选中收藏夹项: %s -> %s", pFavoriteItem->name, pFavoriteItem->path);
                    setCurrentDirectory(pFavoriteItem->path);
                    updateFileList();
                    
                    // 单击同时也展开节点
                    TreeView_Expand(g_treeView, hItem, TVE_EXPAND);
                } else {
                    // 普通目录节点，构建完整路径
                    WCHAR fullPath[MAX_PATH] = {0};
                    getNodeFullPath(g_treeView, hItem, fullPath, MAX_PATH);
                    LogMessage(L"[DEBUG] 节点选中改变: %s", fullPath);
                    
                    // 确保不是在构建包含"★ 收藏夹"的无效路径
                    if (wcsstr(fullPath, L"★ 收藏夹") == NULL) {
                        setCurrentDirectory(fullPath);
                        updateFileList();
                    }
                }
            }
            break;
        }
        
        case LVN_COLUMNCLICK: {
            if (nmhdr->hwndFrom == g_listView) {
                if (g_sorting) return 0;
                NMLISTVIEW* p = (NMLISTVIEW*)lParam;
                static int lastCol = -1;
                static BOOL asc = TRUE;
                if (p->iSubItem == lastCol) asc = !asc; else { lastCol = p->iSubItem; asc = TRUE; }
                
                g_sorting = TRUE;
                SortParam* sp = new SortParam{ p->iSubItem, asc };
                CreateThread(NULL, 0, SortWorker, sp, 0, NULL);
            }
            break;
        }

        case LVN_GETDISPINFO: {
            NMLVDISPINFO* pDispInfo = (NMLVDISPINFO*)lParam;
            if (pDispInfo->hdr.hwndFrom == g_listView) {
                LVITEM& item = pDispInfo->item;
                int index = item.iItem;
                
                EnterCriticalSection(&g_fileListLock);
                if (index >= 0 && index < (int)g_fileList.size()) {
                    const ItemSortData& data = g_fileList[index];
                    
                    if (item.mask & LVIF_TEXT) {
                        switch (item.iSubItem) {
                            case 0: // Name
                                wcsncpy_s(item.pszText, item.cchTextMax, data.name.c_str(), _TRUNCATE);
                                break;
                            case 1: // Size
                                {
                                    WCHAR buf[64];
                                    formatFileSize(data.sizeNumeric, buf, 64);
                                    if (data.isPartial) {
                                        lstrcatW(buf, L"+");
                                    }
                                    wcsncpy_s(item.pszText, item.cchTextMax, buf, _TRUNCATE);
                                }
                                break;
                            case 2: // Type
                                wcsncpy_s(item.pszText, item.cchTextMax, data.isDir ? L"文件夹" : L"文件", _TRUNCATE);
                                break;
                            case 3: // Modified
                                {
                                    WCHAR buf[64];
                                    formatFileTime(&data.modified, buf, 64);
                                    wcsncpy_s(item.pszText, item.cchTextMax, buf, _TRUNCATE);
                                }
                                break;
                            case 4: // Created
                                {
                                    WCHAR buf[64];
                                    formatFileTime(&data.created, buf, 64);
                                    wcsncpy_s(item.pszText, item.cchTextMax, buf, _TRUNCATE);
                                }
                                break;
                        }
                    }
                    
                    if (item.mask & LVIF_IMAGE) {
                        item.iImage = data.isDir ? 0 : 1;
                    }
                }
                LeaveCriticalSection(&g_fileListLock);
            }
            break;
        }
        
        default:
            break;
    }
    
    return 0;
}
