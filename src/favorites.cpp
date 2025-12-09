#include "favorites.h"
#include <shlobj.h>
#include <shlwapi.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <stdio.h>
#include <wchar.h>
#include "log.h"

// 全局变量定义
FavoriteItem g_favorites[MAX_FAVORITES] = { 0 };
int g_favoriteCount = 0;
// HTREEITEM g_favoritesNode = NULL; // 移动到explorer.cpp中定义

// 获取收藏夹文件路径
void getFavoritesFilePath(WCHAR* filePath, int bufferSize) {
    // 尝试获取AppData目录
    if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, filePath))) {
        // 如果获取失败，使用当前目录
        LogMessage(L"[WARN] 无法获取AppData路径，使用当前目录: %s", filePath);
        GetCurrentDirectoryW(bufferSize, filePath);
    }
    
    // 添加收藏夹文件名
    PathAppendW(filePath, FAVORITES_FILE);
    LogMessage(L"[DEBUG] 收藏夹文件路径: %s", filePath);
}

// 将当前路径添加到收藏夹
void addCurrentPathToFavorites() {
    LogMessage(L"[FAVORITES] 开始添加当前路径到收藏夹");
    LogMessage(L"[FAVORITES] 当前路径: %s", g_currentPath);
    
    if (g_favoriteCount >= MAX_FAVORITES) {
        LogMessage(L"[FAVORITES] [WARN] 收藏夹已满，无法添加更多项，当前数量: %d", g_favoriteCount);
        ShowCustomTooltip(g_mainWindow, L"收藏夹已满！");
        return;
    }
    
    // 获取当前目录名称
    WCHAR dirName[MAX_PATH];
    const WCHAR* lastSlash = wcsrchr(g_currentPath, L'\\');
    if (lastSlash && *(lastSlash + 1) != L'\0') {
        wcscpy_s(dirName, MAX_PATH, lastSlash + 1);
    } else {
        wcscpy_s(dirName, MAX_PATH, g_currentPath);
    }
    
    LogMessage(L"[FAVORITES] 尝试添加收藏夹项: %s (%s)", dirName, g_currentPath);
    
    // 检查是否已存在于收藏夹中
    LogMessage(L"[FAVORITES] 检查路径是否已存在于收藏夹中");
    for (int i = 0; i < g_favoriteCount; i++) {
        if (wcscmp(g_favorites[i].path, g_currentPath) == 0) {
            LogMessage(L"[FAVORITES] [INFO] 路径已存在于收藏夹中: %s", g_currentPath);
            ShowCustomTooltip(g_mainWindow, L"已在收藏夹中！");
            return;
        }
    }
    LogMessage(L"[FAVORITES] 路径不存在于收藏夹中，可以添加");
    
    // 添加到收藏夹数组
    wcscpy_s(g_favorites[g_favoriteCount].name, MAX_PATH, dirName);
    wcscpy_s(g_favorites[g_favoriteCount].path, MAX_PATH, g_currentPath);
    g_favoriteCount++;
    
    LogMessage(L"[FAVORITES] 成功添加收藏夹项: %s (%s)，当前总数: %d", 
               dirName, g_currentPath, g_favoriteCount);
    
    // 更新树形视图
    LogMessage(L"[FAVORITES] 开始更新树形视图");
    loadFavoritesIntoTree();
    LogMessage(L"[FAVORITES] 树形视图更新完成");
    
    // 保存到文件
    LogMessage(L"[FAVORITES] 开始保存收藏夹到文件");
    saveFavoritesToFile();
    LogMessage(L"[FAVORITES] 收藏夹保存完成");
    
    ShowCustomTooltip(g_mainWindow, L"已添加到收藏夹！");
    LogMessage(L"[FAVORITES] 添加收藏夹操作完成");
}

// 移除选中的收藏夹项
void removeSelectedFavorite() {
    // 获取选中的树节点
    HTREEITEM hSelectedItem = TreeView_GetSelection(g_treeView);
    if (!hSelectedItem || hSelectedItem == g_favoritesNode) {
        LogMessage(L"[WARN] 未选择有效的收藏夹项");
        return;
    }
    
    // 获取节点参数
    TVITEMW tvi = {0};
    tvi.hItem = hSelectedItem;
    tvi.mask = TVIF_PARAM;
    if (!TreeView_GetItem(g_treeView, &tvi)) {
        LogMessage(L"[ERROR] 无法获取树节点参数");
        return;
    }
    
    // 检查是否为收藏夹项
    FavoriteItem* favoriteItem = (FavoriteItem*)tvi.lParam;
    if (favoriteItem < g_favorites || favoriteItem >= g_favorites + g_favoriteCount) {
        LogMessage(L"[WARN] 选中的项不是收藏夹项");
        return;
    }
    
    // 从数组中移除
    int index = favoriteItem - g_favorites;
    for (int i = index; i < g_favoriteCount - 1; i++) {
        g_favorites[i] = g_favorites[i + 1];
    }
    g_favoriteCount--;
    
    LogMessage(L"[DEBUG] 已从收藏夹移除项，剩余数量: %d", g_favoriteCount);
    
    // 重新加载收藏夹树节点
    loadFavoritesIntoTree();
    
    // 保存到文件
    saveFavoritesToFile();
    
    ShowCustomTooltip(g_mainWindow, L"已从收藏夹移除！");
}

// 保存收藏夹到文件（JSON格式）
void saveFavoritesToFile() {
    LogMessage(L"[FAVORITES] [SAVE] 开始保存收藏夹到JSON文件");
    LogMessage(L"[FAVORITES] [SAVE] 当前收藏夹数量: %d", g_favoriteCount);
    
    // 获取收藏夹文件路径
    WCHAR filePath[MAX_PATH];
    getFavoritesFilePath(filePath, MAX_PATH);
    LogMessage(L"[FAVORITES] [SAVE] 保存收藏夹到JSON文件: %s", filePath);
    
    // 确保目录存在
    WCHAR dirPath[MAX_PATH];
    wcscpy_s(dirPath, MAX_PATH, filePath);
    WCHAR* lastSlash = wcsrchr(dirPath, L'\\');
    if (lastSlash != NULL) {
        *lastSlash = L'\0';
        if (!CreateDirectoryW(dirPath, NULL)) {
            DWORD err = GetLastError();
            if (err != ERROR_ALREADY_EXISTS) {
                LogMessage(L"[WARN] 无法创建目录 %s (错误: %d)", dirPath, err);
            }
        }
    }
    
    HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        LogMessage(L"[FAVORITES] [SAVE] [ERROR] 无法创建收藏夹JSON文件，错误代码: %d", error);
        return;
    }
    LogMessage(L"[FAVORITES] [SAVE] 文件创建成功");
    
    // 构建JSON字符串
    WCHAR jsonBuffer[4096];  // JSON缓冲区
    int pos = 0;
    
    // 写入JSON开始
    pos += swprintf_s(jsonBuffer + pos, 4096 - pos, L"{\"version\": 1, \"favorites\": [\n");
    
    // 写入每个收藏夹项
    for (int i = 0; i < g_favoriteCount; i++) {
        // 转义特殊字符
        WCHAR escapedName[MAX_PATH * 2] = {0};
        WCHAR escapedPath[MAX_PATH * 2] = {0};
        
        // 简单的转义处理
        const WCHAR* srcName = g_favorites[i].name;
        WCHAR* dstName = escapedName;
        while (*srcName && dstName - escapedName < MAX_PATH * 2 - 2) {
            if (*srcName == L'\\' || *srcName == L'\"') {
                *dstName++ = L'\\';
            }
            *dstName++ = *srcName++;
        }
        *dstName = L'\0';
        
        const WCHAR* srcPath = g_favorites[i].path;
        WCHAR* dstPath = escapedPath;
        while (*srcPath && dstPath - escapedPath < MAX_PATH * 2 - 2) {
            if (*srcPath == L'\\' || *srcPath == L'\"') {
                *dstPath++ = L'\\';
            }
            *dstPath++ = *srcPath++;
        }
        *dstPath = L'\0';
        
        // 写入JSON对象
        pos += swprintf_s(jsonBuffer + pos, 4096 - pos, 
            L"  {\"name\": \"%s\", \"path\": \"%s\"}", 
            escapedName, escapedPath);
        
        // 如果不是最后一项，添加逗号
        if (i < g_favoriteCount - 1) {
            pos += swprintf_s(jsonBuffer + pos, 4096 - pos, L",\n");
        } else {
            pos += swprintf_s(jsonBuffer + pos, 4096 - pos, L"\n");
        }
        
        LogMessage(L"[FAVORITES] [SAVE] 写入JSON收藏夹项 %d: %s -> %s", i, g_favorites[i].name, g_favorites[i].path);
    }
    
    // 写入JSON结束
    pos += swprintf_s(jsonBuffer + pos, 4096 - pos, L"]}\n");
    LogMessage(L"[FAVORITES] [SAVE] JSON构建完成，总长度: %d 字符", pos);
    
    // 写入文件
    DWORD bytesWritten;
    // 写入UTF-8 BOM（可选，但推荐用于UTF-8文件）
    const BYTE utf8Bom[] = {0xEF, 0xBB, 0xBF};
    WriteFile(hFile, utf8Bom, sizeof(utf8Bom), &bytesWritten, NULL);
    LogMessage(L"[FAVORITES] [SAVE] UTF-8 BOM写入完成，写入字节数: %d", bytesWritten);
    
    // 将WCHAR转换为UTF-8
    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, jsonBuffer, -1, NULL, 0, NULL, NULL);
    LogMessage(L"[FAVORITES] [SAVE] UTF-8转换大小: %d 字节", utf8Size);
    
    if (utf8Size > 0) {
        char* utf8Buffer = (char*)malloc(utf8Size);
        if (utf8Buffer) {
            WideCharToMultiByte(CP_UTF8, 0, jsonBuffer, -1, utf8Buffer, utf8Size, NULL, NULL);
            WriteFile(hFile, utf8Buffer, utf8Size - 1, &bytesWritten, NULL);  // -1 排除null终止符
            LogMessage(L"[FAVORITES] [SAVE] JSON内容写入完成，写入字节数: %d", bytesWritten);
            free(utf8Buffer);
        } else {
            LogMessage(L"[FAVORITES] [SAVE] [ERROR] 内存分配失败");
        }
    } else {
        LogMessage(L"[FAVORITES] [SAVE] [ERROR] UTF-8转换失败");
    }
    
    CloseHandle(hFile);
    LogMessage(L"[FAVORITES] [SAVE] JSON格式收藏夹保存完成，共保存 %d 项", g_favoriteCount);
}

// 从文件加载收藏夹（JSON格式）
void loadFavoritesFromFile() {
    LogMessage(L"[FAVORITES] [LOAD] 开始从JSON文件加载收藏夹");
    
    // 获取收藏夹文件路径
    WCHAR filePath[MAX_PATH];
    getFavoritesFilePath(filePath, MAX_PATH);
    LogMessage(L"[FAVORITES] [LOAD] 收藏夹文件路径: %s", filePath);
    
    // 检查文件是否存在
    DWORD fileAttributes = GetFileAttributesW(filePath);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        LogMessage(L"[FAVORITES] [LOAD] 收藏夹文件不存在，错误代码: %d，创建空的收藏夹文件", error);
        g_favoriteCount = 0;
        
        // 创建空的收藏夹文件
        saveFavoritesToFile();
        return;
    }
    LogMessage(L"[FAVORITES] [LOAD] 文件存在，文件属性: 0x%X", fileAttributes);
    
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        LogMessage(L"[FAVORITES] [LOAD] [ERROR] 无法打开收藏夹文件，错误代码: %d", error);
        return;
    }
    
    // 获取文件大小
    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        DWORD error = GetLastError();
        LogMessage(L"[FAVORITES] [LOAD] [ERROR] 无法获取JSON文件大小，错误代码: %d", error);
        CloseHandle(hFile);
        return;
    }
    LogMessage(L"[FAVORITES] [LOAD] 文件大小: %d 字节", fileSize);
    
    // 读取文件内容
    char* fileBuffer = (char*)malloc(fileSize + 1);
    if (!fileBuffer) {
        LogMessage(L"[FAVORITES] [LOAD] [ERROR] 内存分配失败");
        CloseHandle(hFile);
        return;
    }
    LogMessage(L"[FAVORITES] [LOAD] 内存分配成功，缓冲区大小: %d 字节", fileSize + 1);
    
    DWORD bytesRead;
    if (!ReadFile(hFile, fileBuffer, fileSize, &bytesRead, NULL)) {
        DWORD error = GetLastError();
        LogMessage(L"[FAVORITES] [LOAD] [ERROR] 读取文件失败，错误代码: %d", error);
        free(fileBuffer);
        CloseHandle(hFile);
        return;
    }
    LogMessage(L"[FAVORITES] [LOAD] 文件读取成功，读取字节数: %d", bytesRead);
    fileBuffer[fileSize] = '\0';
    CloseHandle(hFile);
    
    // 跳过UTF-8 BOM（如果存在）
    char* jsonStart = fileBuffer;
    if (fileSize >= 3 && (unsigned char)fileBuffer[0] == 0xEF && 
        (unsigned char)fileBuffer[1] == 0xBB && (unsigned char)fileBuffer[2] == 0xBF) {
        jsonStart = fileBuffer + 3;
        LogMessage(L"[FAVORITES] [LOAD] 检测到UTF-8 BOM，跳过BOM后文件大小: %d 字节", fileSize - 3);
    } else {
        LogMessage(L"[FAVORITES] [LOAD] 未检测到UTF-8 BOM，使用完整文件内容");
    }
    
    // 将UTF-8转换为WCHAR
    int wcharSize = MultiByteToWideChar(CP_UTF8, 0, jsonStart, -1, NULL, 0);
    LogMessage(L"[FAVORITES] [LOAD] UTF-8到WCHAR转换大小: %d 字符", wcharSize);
    
    if (wcharSize <= 0) {
        DWORD error = GetLastError();
        LogMessage(L"[FAVORITES] [LOAD] [ERROR] JSON文件内容转换失败，错误代码: %d", error);
        free(fileBuffer);
        return;
    }
    
    WCHAR* jsonWide = (WCHAR*)malloc(wcharSize * sizeof(WCHAR));
    if (!jsonWide) {
        LogMessage(L"[FAVORITES] [LOAD] [ERROR] 内存分配失败");
        free(fileBuffer);
        return;
    }
    LogMessage(L"[FAVORITES] [LOAD] WCHAR缓冲区分配成功，大小: %d 字符", wcharSize);
    
    MultiByteToWideChar(CP_UTF8, 0, jsonStart, -1, jsonWide, wcharSize);
    LogMessage(L"[FAVORITES] [LOAD] UTF-8到WCHAR转换完成");
    
    LogMessage(L"[FAVORITES] [LOAD] JSON文件内容: %s", jsonWide);
    
    // 简单的JSON解析
    g_favoriteCount = 0;
    const WCHAR* current = jsonWide;
    
    // 添加调试信息，显示JSON内容长度
    LogMessage(L"[FAVORITES] [LOAD] JSON内容长度: %d 字符", wcslen(jsonWide));
    
    // 查找"favorites"数组
    const WCHAR* favoritesStart = wcsstr(current, L"\"favorites\":");
    if (!favoritesStart) {
        LogMessage(L"[FAVORITES] [LOAD] [ERROR] JSON格式错误：未找到favorites数组");
        free(jsonWide);
        free(fileBuffer);
        return;
    }
    LogMessage(L"[FAVORITES] [LOAD] 找到favorites数组位置");
    
    current = favoritesStart + wcslen(L"\"favorites\":");
    
    // 跳过空白字符
    while (*current && (*current == L' ' || *current == L'\t' || *current == L'\n' || *current == L'\r')) {
        current++;
    }
    
    // 检查数组开始
    if (*current != L'[') {
        LogMessage(L"[FAVORITES] [LOAD] [ERROR] JSON格式错误：favorites后不是数组开始");
        free(jsonWide);
        free(fileBuffer);
        return;
    }
    LogMessage(L"[FAVORITES] [LOAD] 找到favorites数组开始标记'['");
    current++;
    
    // 解析数组中的每个对象
    int favoriteIndex = 0;
    LogMessage(L"[FAVORITES] [LOAD] 开始解析收藏夹数组");
    
    while (*current && g_favoriteCount < MAX_FAVORITES) {
        // 跳过空白字符
        while (*current && (*current == L' ' || *current == L'\t' || *current == L'\n' || *current == L'\r')) {
            current++;
        }
        
        // 检查数组结束
        if (*current == L']') {
            LogMessage(L"[FAVORITES] [LOAD] 检测到数组结束标记']'");
            break;
        }
        
        if (*current == L',') {
            current++;  // 跳过逗号
            LogMessage(L"[FAVORITES] [LOAD] 跳过逗号分隔符");
            continue;
        }
        
        // 检查对象开始
        if (*current != L'{') {
            LogMessage(L"[FAVORITES] [LOAD] [ERROR] JSON格式错误：期望对象开始符'{'，当前字符: 0x%X", *current);
            break;
        }
        LogMessage(L"[FAVORITES] [LOAD] 找到收藏项对象开始标记'{'，索引: %d", favoriteIndex);
        current++;
        favoriteIndex++;
        
        // 解析name字段
        const WCHAR* nameStart = wcsstr(current, L"\"name\"");
        if (!nameStart) {
            LogMessage(L"[FAVORITES] [LOAD] [ERROR] JSON格式错误：未找到name字段");
            break;
        }
        LogMessage(L"[FAVORITES] [LOAD] 找到name字段位置");
        
        // 跳过字段名和冒号
        nameStart += wcslen(L"\"name\"");
        
        // 跳过空白字符和冒号
        while (*nameStart && (*nameStart == L' ' || *nameStart == L'\t' || *nameStart == L':' || *nameStart == L'\n' || *nameStart == L'\r')) {
            nameStart++;
        }
        
        // 检查并跳过引号
        if (*nameStart == L'\"') {
            nameStart++;
        } else {
            LogMessage(L"[FAVORITES] [LOAD] [ERROR] JSON格式错误：name字段值不是字符串");
            break;
        }
        
        const WCHAR* nameEnd = nameStart;
        while (*nameEnd && *nameEnd != L'\"') {
            // 处理转义字符
            if (*nameEnd == L'\\' && *(nameEnd + 1) == L'\"') {
                nameEnd += 2;
                LogMessage(L"[FAVORITES] [LOAD] 跳过转义字符");
            } else {
                nameEnd++;
            }
        }
        LogMessage(L"[FAVORITES] [LOAD] name字段值: %.*s", (int)(nameEnd - nameStart), nameStart);
        
        if (!*nameEnd) {
            LogMessage(L"[WARN] JSON格式错误：name字段未正确结束");
            break;
        }
        
        // 解析path字段
        const WCHAR* pathStart = wcsstr(nameEnd, L"\"path\"");
        if (!pathStart) {
            LogMessage(L"[FAVORITES] [LOAD] [ERROR] JSON格式错误：未找到path字段");
            break;
        }
        LogMessage(L"[FAVORITES] [LOAD] 找到path字段位置");
        
        // 跳过字段名和冒号
        pathStart += wcslen(L"\"path\"");
        
        // 跳过空白字符和冒号
        while (*pathStart && (*pathStart == L' ' || *pathStart == L'\t' || *pathStart == L':' || *pathStart == L'\n' || *pathStart == L'\r')) {
            pathStart++;
        }
        
        // 检查并跳过引号
        if (*pathStart == L'\"') {
            pathStart++;
        } else {
            LogMessage(L"[FAVORITES] [LOAD] [ERROR] JSON格式错误：path字段值不是字符串");
            break;
        }
        
        const WCHAR* pathEnd = pathStart;
        while (*pathEnd && *pathEnd != L'\"') {
            // 处理转义字符
            if (*pathEnd == L'\\' && *(pathEnd + 1) == L'\"') {
                pathEnd += 2;
                LogMessage(L"[FAVORITES] [LOAD] 跳过转义字符");
            } else {
                pathEnd++;
            }
        }
        
        if (!*pathEnd) {
            LogMessage(L"[FAVORITES] [LOAD] [ERROR] JSON格式错误：path字段未正确结束");
            break;
        }
        LogMessage(L"[FAVORITES] [LOAD] path字段值: %.*s", (int)(pathEnd - pathStart), pathStart);
        
        // 复制name和path（处理转义字符）
        WCHAR* nameDst = g_favorites[g_favoriteCount].name;
        const WCHAR* nameSrc = nameStart;
        while (nameSrc < nameEnd && nameDst - g_favorites[g_favoriteCount].name < MAX_PATH - 1) {
            if (*nameSrc == L'\\' && *(nameSrc + 1) == L'\"') {
                *nameDst++ = L'\"';
                nameSrc += 2;
            } else if (*nameSrc == L'\\' && *(nameSrc + 1) == L'\\') {
                *nameDst++ = L'\\';
                nameSrc += 2;
            } else {
                *nameDst++ = *nameSrc++;
            }
        }
        *nameDst = L'\0';
        
        WCHAR* pathDst = g_favorites[g_favoriteCount].path;
        const WCHAR* pathSrc = pathStart;
        while (pathSrc < pathEnd && pathDst - g_favorites[g_favoriteCount].path < MAX_PATH - 1) {
            if (*pathSrc == L'\\' && *(pathSrc + 1) == L'\"') {
                *pathDst++ = L'\"';
                pathSrc += 2;
            } else if (*pathSrc == L'\\' && *(pathSrc + 1) == L'\\') {
                *pathDst++ = L'\\';
                pathSrc += 2;
            } else {
                *pathDst++ = *pathSrc++;
            }
        }
        *pathDst = L'\0';
        
        LogMessage(L"[FAVORITES] [LOAD] 解析JSON收藏夹项 %d: %s -> %s", 
                   g_favoriteCount, g_favorites[g_favoriteCount].name, g_favorites[g_favoriteCount].path);
        
        g_favoriteCount++;
        LogMessage(L"[FAVORITES] [LOAD] 收藏项计数增加到: %d", g_favoriteCount);
        current = pathEnd + 1;
        
        // 跳过空白字符，找到对象结束符
        while (*current && (*current == L' ' || *current == L'\t' || *current == L'\n' || *current == L'\r')) {
            current++;
        }
        
        // 检查并跳过对象结束符'}'
        if (*current == L'}') {
            current++;
            LogMessage(L"[FAVORITES] [LOAD] 跳过对象结束符'}'");
        } else {
            LogMessage(L"[FAVORITES] [LOAD] [WARN] 期望对象结束符'}'，但找到字符: 0x%X", *current);
        }
        
        // 跳过逗号（如果有）
        while (*current && (*current == L' ' || *current == L'\t' || *current == L'\n' || *current == L'\r')) {
            current++;
        }
        if (*current == L',') {
            current++;
        }
    }
    
    free(jsonWide);
    free(fileBuffer);
    LogMessage(L"[FAVORITES] [LOAD] JSON格式收藏夹加载完成，共加载 %d 项", g_favoriteCount);
}

// 根据路径从收藏夹中移除项
void removeFavoriteByPath(const WCHAR* path) {
    LogMessage(L"[FAVORITES] [REMOVE] 开始根据路径移除收藏项: %s", path);
    
    // 查找匹配的收藏夹项
    for (int i = 0; i < g_favoriteCount; i++) {
        if (wcscmp(g_favorites[i].path, path) == 0) {
            LogMessage(L"[FAVORITES] [REMOVE] 找到匹配的收藏项 %d: %s", i, g_favorites[i].name);
            
            // 从数组中移除
            for (int j = i; j < g_favoriteCount - 1; j++) {
                g_favorites[j] = g_favorites[j + 1];
            }
            g_favoriteCount--;
            LogMessage(L"[FAVORITES] [REMOVE] 收藏项移除完成，当前计数: %d", g_favoriteCount);
            
            // 重新加载收藏夹树节点
            loadFavoritesIntoTree();
            LogMessage(L"[FAVORITES] [REMOVE] 树节点重新加载完成");
            break;
        }
    }
    
    LogMessage(L"[FAVORITES] [REMOVE] 移除操作完成");
}

// 创建收藏夹节点
HTREEITEM createFavoritesNode() {
    LogMessage(L"[DEBUG] createFavoritesNode 开始\n");
    
    if (!g_treeView) {
        LogMessage(L"[ERROR] g_treeView 为空\n");
        return NULL;
    }
    
    // 创建收藏夹根节点
    TVINSERTSTRUCTW tvInsert = {0};
    tvInsert.hParent = NULL;
    tvInsert.hInsertAfter = TVI_FIRST;
    tvInsert.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvInsert.item.pszText = (LPWSTR)L"★ 收藏夹";
    tvInsert.item.lParam = (LPARAM)FAVORITE_ITEM_MARKER;  // 特殊标记
    
    HTREEITEM favoritesNode = TreeView_InsertItem(g_treeView, &tvInsert);
    
    if (favoritesNode) {
        LogMessage(L"[DEBUG] 收藏夹节点创建成功\n");
    } else {
        LogMessage(L"[ERROR] 收藏夹节点创建失败\n");
        return NULL;
    }
    
    // 添加一个占位符子节点，使节点可以展开
    TVINSERTSTRUCTW tvPlaceholder = {0};
    tvPlaceholder.hParent = favoritesNode;
    tvPlaceholder.hInsertAfter = TVI_LAST;
    tvPlaceholder.item.mask = TVIF_TEXT;
    tvPlaceholder.item.pszText = (LPWSTR)L" ";
    
    HTREEITEM hPlaceholder = TreeView_InsertItem(g_treeView, &tvPlaceholder);
    if (hPlaceholder) {
        LogMessage(L"[DEBUG] 占位符节点添加成功\n");
    } else {
        LogMessage(L"[ERROR] 占位符节点添加失败\n");
    }
    
    LogMessage(L"[DEBUG] createFavoritesNode 完成\n");
    return favoritesNode;
}

// 加载收藏夹项到树中
void loadFavoritesIntoTree() {
    // 清空现有的收藏夹子项
    HTREEITEM hChild = TreeView_GetChild(g_treeView, g_favoritesNode);
    while (hChild) {
        HTREEITEM hNext = TreeView_GetNextSibling(g_treeView, hChild);
        TreeView_DeleteItem(g_treeView, hChild);
        hChild = hNext;
    }
    
    // 添加所有收藏夹项
    for (int i = 0; i < g_favoriteCount; i++) {
        TVINSERTSTRUCTW tvInsert = {0};
        tvInsert.hParent = g_favoritesNode;
        tvInsert.hInsertAfter = TVI_LAST;
        tvInsert.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvInsert.item.pszText = g_favorites[i].name;
        tvInsert.item.lParam = (LPARAM)&g_favorites[i];  // 存储指向收藏夹项的指针
        
        TreeView_InsertItem(g_treeView, &tvInsert);
    }
}

// 编辑选中收藏夹项的名称
void editFavoriteName() {
    LogMessage(L"[DEBUG] editFavoriteName 开始执行");
    
    // 获取选中的树节点
    HTREEITEM hSelectedItem = TreeView_GetSelection(g_treeView);
    if (!hSelectedItem) {
        LogMessage(L"[ERROR] editFavoriteName: 没有选中的树节点");
        MessageBoxW(g_mainWindow, L"请先选择一个收藏夹项", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }
    LogMessage(L"[DEBUG] editFavoriteName: 获取到选中的树节点");
    
    // 获取节点的数据
    TVITEMW tvItem = {0};
    tvItem.hItem = hSelectedItem;
    tvItem.mask = TVIF_PARAM;
    if (!TreeView_GetItem(g_treeView, &tvItem)) {
        LogMessage(L"[ERROR] editFavoriteName: 无法获取树节点数据");
        MessageBoxW(g_mainWindow, L"无法获取选中项的数据", L"错误", MB_OK | MB_ICONERROR);
        return;
    }
    LogMessage(L"[DEBUG] editFavoriteName: 成功获取树节点数据");
    
    // 检查是否为收藏夹项
    FavoriteItem* favoriteItem = (FavoriteItem*)tvItem.lParam;
    
    // 更加宽松的验证，或者只依靠前面的逻辑
    BOOL isValid = FALSE;
    for(int i=0; i<g_favoriteCount; i++) {
        if(&g_favorites[i] == favoriteItem) {
            isValid = TRUE;
            break;
        }
    }

    if (!isValid) {
        LogMessage(L"[ERROR] editFavoriteName: 选中的不是收藏夹项");
        LogMessage(L"[DEBUG] favoriteItem指针: %p, g_favorites起始: %p, g_favoriteCount: %d", 
                   favoriteItem, g_favorites, g_favoriteCount);
        MessageBoxW(g_mainWindow, L"请选择一个有效的收藏夹项", L"错误", MB_OK | MB_ICONERROR);
        return;  // 不是收藏夹项
    }
    LogMessage(L"[DEBUG] editFavoriteName: 确认是收藏夹项，名称: %s", favoriteItem->name);
    
    // 创建输入对话框
    WCHAR newName[MAX_PATH];
    wcscpy_s(newName, MAX_PATH, favoriteItem->name);
    LogMessage(L"[DEBUG] editFavoriteName: 准备调用showInputDialog");
    
    if (showInputDialog(g_mainWindow, L"编辑收藏夹名称", L"请输入新的名称:", newName, MAX_PATH)) {
        LogMessage(L"[DEBUG] editFavoriteName: 对话框返回确定，新名称: %s", newName);
        
        // 检查名称是否为空
        if (wcslen(newName) == 0) {
            LogMessage(L"[ERROR] editFavoriteName: 新名称为空");
            MessageBoxW(g_mainWindow, L"名称不能为空", L"错误", MB_OK | MB_ICONERROR);
            return;
        }
        
        // 检查名称是否与原来相同
        if (wcscmp(newName, favoriteItem->name) == 0) {
            LogMessage(L"[DEBUG] editFavoriteName: 名称未改变，无需更新");
            MessageBoxW(g_mainWindow, L"名称未改变", L"提示", MB_OK | MB_ICONINFORMATION);
            return;
        }
        
        // 更新收藏夹项的名称
        wcscpy_s(favoriteItem->name, MAX_PATH, newName);
        LogMessage(L"[DEBUG] editFavoriteName: 收藏夹项名称已更新");
        
        // 重新加载收藏夹树
        loadFavoritesIntoTree();
        LogMessage(L"[DEBUG] editFavoriteName: 收藏夹树已重新加载");
        
        // 保存到文件
        saveFavoritesToFile();
        LogMessage(L"[DEBUG] editFavoriteName: 数据已保存到文件");
        
        // 显示成功消息
        MessageBoxW(g_mainWindow, L"收藏夹名称修改成功", L"成功", MB_OK | MB_ICONINFORMATION);
    } else {
        LogMessage(L"[DEBUG] editFavoriteName: 对话框返回取消或失败");
    }
    
    LogMessage(L"[DEBUG] editFavoriteName: 函数执行完成");
}

// 对话框状态结构
struct InputDialogState {
    LPWSTR textBuffer;
    int bufferSize;
    BOOL* pUserPressedOK;
    HWND hwndEdit;
};

// 对话框窗口过程
LRESULT CALLBACK InputDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    InputDialogState* pState = (InputDialogState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg) {
    case WM_COMMAND:
        if (pState) {
            int id = LOWORD(wParam);
            if (id == IDOK) {
                if (pState->hwndEdit) {
                    GetWindowTextW(pState->hwndEdit, pState->textBuffer, pState->bufferSize);
                }
                *(pState->pUserPressedOK) = TRUE;
                DestroyWindow(hwnd);
                return 0;
            } else if (id == IDCANCEL) {
                *(pState->pUserPressedOK) = FALSE;
                DestroyWindow(hwnd);
                return 0;
            }
        }
        break;

    case WM_CLOSE:
        if (pState && pState->pUserPressedOK) {
             *(pState->pUserPressedOK) = FALSE;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
        
    case WM_CTLCOLORSTATIC:
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// 简单的输入对话框实现
BOOL showInputDialog(HWND hwndOwner, LPCWSTR title, LPCWSTR prompt, LPWSTR text, int textSize) {
    LogMessage(L"[DEBUG] showInputDialog 开始执行，标题: %s，提示: %s", title, prompt);
    
    // 注册窗口类
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = InputDlgProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ExplorerInputDlg";
    
    RegisterClassExW(&wc);
    
    // 获取屏幕尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 窗口大小
    int dlgWidth = 320;
    int dlgHeight = 150;
    
    // 计算居中位置
    int x = (screenWidth - dlgWidth) / 2;
    int y = (screenHeight - dlgHeight) / 2;
    
    // 准备状态数据
    BOOL userPressedOK = FALSE;
    InputDialogState state = { text, textSize, &userPressedOK, NULL };
    
    // 创建对话框窗口
    HWND hwndDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"ExplorerInputDlg", title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, dlgWidth, dlgHeight,
        hwndOwner, NULL, GetModuleHandle(NULL), NULL);
    
    if (!hwndDlg) {
        LogMessage(L"[ERROR] showInputDialog: 对话框窗口创建失败，错误码: %d", GetLastError());
        return FALSE;
    }
    
    // 设置状态指针
    SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)&state);
    
    LogMessage(L"[DEBUG] showInputDialog: 对话框窗口创建成功，窗口句柄: %p", hwndDlg);
    
    // 创建字体 (Segoe UI, 9pt)
    HFONT hFont = CreateFontW(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        
    if (!hFont) {
        hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }
    
    // 创建控件
    HWND hwndPrompt = CreateWindowExW(0, L"STATIC", prompt,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        15, 15, 280, 20, hwndDlg, NULL, GetModuleHandle(NULL), NULL);
    
    state.hwndEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        15, 40, 275, 25, hwndDlg, (HMENU)1001, GetModuleHandle(NULL), NULL);
    
    HWND hwndOK = CreateWindowExW(0, L"BUTTON", L"确定",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        90, 80, 70, 25, hwndDlg, (HMENU)IDOK, GetModuleHandle(NULL), NULL);
    
    HWND hwndCancel = CreateWindowExW(0, L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        170, 80, 70, 25, hwndDlg, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);
    
    if (!hwndPrompt || !state.hwndEdit || !hwndOK || !hwndCancel) {
        LogMessage(L"[ERROR] showInputDialog: 控件创建失败，错误码: %d", GetLastError());
        DestroyWindow(hwndDlg);
        if (hFont != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(hFont);
        return FALSE;
    }
    
    // 设置字体
    SendMessageW(hwndPrompt, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessageW(state.hwndEdit, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessageW(hwndOK, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessageW(hwndCancel, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    
    // 设置焦点到编辑框并选中所有文本
    SetFocus(state.hwndEdit);
    SendMessageW(state.hwndEdit, EM_SETSEL, 0, -1);
    
    // 设置对话框为模态窗口
    EnableWindow(hwndOwner, FALSE);
    
    // 消息循环
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hwndDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    
    // 恢复主窗口
    EnableWindow(hwndOwner, TRUE);
    SetFocus(hwndOwner);
    
    if (hFont != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(hFont);
    
    return userPressedOK;
}