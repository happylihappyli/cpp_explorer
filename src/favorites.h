#ifndef FAVORITES_H
#define FAVORITES_H

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <wchar.h>

// 前向声明
void ShowCustomTooltip(HWND parent, const WCHAR* text);



// 全局变量声明
extern HWND g_mainWindow;
extern HWND g_treeView;
extern WCHAR g_currentPath[MAX_PATH];

// 收藏夹条目结构
typedef struct {
    WCHAR name[MAX_PATH];
    WCHAR path[MAX_PATH];
} FavoriteItem;

// 最大收藏夹数量
#define MAX_FAVORITES 100

// 收藏夹文件名
#define FAVORITES_FILE L"favorites.json"

// 收藏夹项标识符
#define FAVORITE_ITEM_MARKER ((LPARAM)0x12345678)



// 全局变量声明
extern FavoriteItem g_favorites[MAX_FAVORITES];
extern int g_favoriteCount;
extern HTREEITEM g_favoritesNode;

// 函数声明
void getFavoritesFilePath(WCHAR* filePath, int bufferSize);
void loadFavoritesIntoTree();
void saveFavoritesToFile();
void loadFavoritesFromFile();
void addCurrentPathToFavorites();
void removeSelectedFavorite();
void editFavoriteName();
void removeFavoriteByPath(const WCHAR* path);
HTREEITEM createFavoritesNode();

// 对话框函数声明
BOOL showInputDialog(HWND hwndOwner, LPCWSTR title, LPCWSTR prompt, LPWSTR text, int textSize);

#endif // FAVORITES_H