#include "notification_handlers.h"
#include "favorites.h"
#include "tree_utils.h"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <wchar.h>
#include "log.h"

// 外部变量声明
extern HWND g_listView;
extern HWND g_treeView;
extern HWND g_mainWindow;
extern HWND g_addressBar;
extern HTREEITEM g_favoritesNode;
extern WCHAR g_currentPath[MAX_PATH];

// 函数声明
void HandleListViewDoubleClick(HWND hwnd, LPARAM lParam);
void HandleTreeViewDoubleClick(HWND hwnd, HWND mainWindow);
void setCurrentDirectory(const WCHAR* path);
void updateFileList();
void loadFavoritesIntoTree();


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
        
        default:
            break;
    }
    
    return 0;
}