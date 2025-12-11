#include "file_utils.h"
#include "log.h"
#include <windows.h>
#include <shlwapi.h>
#include <string>

static HMODULE g_sqliteLib = NULL;
static bool g_sqliteLoaded = false;
typedef void sqlite3;
typedef void sqlite3_stmt;
typedef int (__cdecl *pf_sqlite3_open)(const char*, sqlite3**);
typedef int (__cdecl *pf_sqlite3_close)(sqlite3*);
typedef int (__cdecl *pf_sqlite3_exec)(sqlite3*, const char*, int (*)(void*,int,char**,char**), void*, char**);
typedef int (__cdecl *pf_sqlite3_prepare_v2)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
typedef int (__cdecl *pf_sqlite3_step)(sqlite3_stmt*);
typedef int (__cdecl *pf_sqlite3_finalize)(sqlite3_stmt*);
typedef int (__cdecl *pf_sqlite3_bind_text)(sqlite3_stmt*, int, const char*, int, void(*)(void*));
typedef int (__cdecl *pf_sqlite3_bind_int64)(sqlite3_stmt*, int, long long);
typedef const unsigned char* (__cdecl *pf_sqlite3_column_text)(sqlite3_stmt*, int);
typedef long long (__cdecl *pf_sqlite3_column_int64)(sqlite3_stmt*, int);
typedef const char* (__cdecl *pf_sqlite3_errmsg)(sqlite3*);

static pf_sqlite3_open p_sqlite3_open = NULL;
static pf_sqlite3_close p_sqlite3_close = NULL;
static pf_sqlite3_exec p_sqlite3_exec = NULL;
static pf_sqlite3_prepare_v2 p_sqlite3_prepare_v2 = NULL;
static pf_sqlite3_step p_sqlite3_step = NULL;
static pf_sqlite3_finalize p_sqlite3_finalize = NULL;
static pf_sqlite3_bind_text p_sqlite3_bind_text = NULL;
static pf_sqlite3_bind_int64 p_sqlite3_bind_int64 = NULL;
static pf_sqlite3_column_text p_sqlite3_column_text = NULL;
static pf_sqlite3_column_int64 p_sqlite3_column_int64 = NULL;
static pf_sqlite3_errmsg p_sqlite3_errmsg = NULL;

static bool loadSqliteDll() {
    if (g_sqliteLoaded) return true;
    WCHAR dllPath[MAX_PATH];
    getExecutableDirectory(dllPath, MAX_PATH);
    lstrcatW(dllPath, L"sqlite3.dll");
    HMODULE h = LoadLibraryW(dllPath);
    if (!h) h = LoadLibraryW(L"sqlite3.dll");
    if (!h) return false;
    g_sqliteLib = h;
    p_sqlite3_open = (pf_sqlite3_open)GetProcAddress(h, "sqlite3_open");
    p_sqlite3_close = (pf_sqlite3_close)GetProcAddress(h, "sqlite3_close");
    p_sqlite3_exec = (pf_sqlite3_exec)GetProcAddress(h, "sqlite3_exec");
    p_sqlite3_prepare_v2 = (pf_sqlite3_prepare_v2)GetProcAddress(h, "sqlite3_prepare_v2");
    p_sqlite3_step = (pf_sqlite3_step)GetProcAddress(h, "sqlite3_step");
    p_sqlite3_finalize = (pf_sqlite3_finalize)GetProcAddress(h, "sqlite3_finalize");
    p_sqlite3_bind_text = (pf_sqlite3_bind_text)GetProcAddress(h, "sqlite3_bind_text");
    p_sqlite3_bind_int64 = (pf_sqlite3_bind_int64)GetProcAddress(h, "sqlite3_bind_int64");
    p_sqlite3_column_text = (pf_sqlite3_column_text)GetProcAddress(h, "sqlite3_column_text");
    p_sqlite3_column_int64 = (pf_sqlite3_column_int64)GetProcAddress(h, "sqlite3_column_int64");
    p_sqlite3_errmsg = (pf_sqlite3_errmsg)GetProcAddress(h, "sqlite3_errmsg");
    g_sqliteLoaded = p_sqlite3_open && p_sqlite3_close && p_sqlite3_exec && p_sqlite3_prepare_v2 && p_sqlite3_step && p_sqlite3_finalize && p_sqlite3_bind_text && p_sqlite3_bind_int64 && p_sqlite3_column_text && p_sqlite3_column_int64;
    return g_sqliteLoaded;
}

static std::string WideToUtf8(const WCHAR* w) {
    if (!w) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    std::string out;
    out.resize(len > 0 ? (len - 1) : 0);
    if (len > 0) {
        WideCharToMultiByte(CP_UTF8, 0, w, -1, &out[0], len, NULL, NULL);
    }
    return out;
}

static bool sqliteOpenDb(sqlite3** dbOut) {
    WCHAR dbPathW[MAX_PATH];
    getExecutableDirectory(dbPathW, MAX_PATH);
    lstrcatW(dbPathW, L"dir_sizes.db");
    std::string dbPath = WideToUtf8(dbPathW);
    return p_sqlite3_open && p_sqlite3_open(dbPath.c_str(), dbOut) == 0;
}

static void sqliteEnsureTable(sqlite3* db) {
    const char* sql = "CREATE TABLE IF NOT EXISTS dir_sizes(path TEXT PRIMARY KEY, size INTEGER, updated_at TEXT)";
    if (p_sqlite3_exec) p_sqlite3_exec(db, sql, NULL, NULL, NULL);
}

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

// 获取可执行文件所在目录
void getExecutableDirectory(WCHAR* buffer, int bufferSize) {
    if (GetModuleFileNameW(NULL, buffer, bufferSize) == 0) {
        // 失败时使用当前目录
        GetCurrentDirectoryW(bufferSize, buffer);
    } else {
        // 移除文件名，只保留目录
        PathRemoveFileSpecW(buffer);
    }
    
    // 确保以反斜杠结尾
    int len = lstrlenW(buffer);
    if (len > 0 && buffer[len - 1] != L'\\') {
        lstrcatW(buffer, L"\\");
    }
}

// 计算目录累计大小（递归）
ULONGLONG computeDirectorySize(const WCHAR* path) {
    ULONGLONG total = 0;

    WCHAR searchPath[MAX_PATH];
    lstrcpyW(searchPath, path);
    int len = lstrlenW(searchPath);
    if (len > 0 && searchPath[len - 1] != L'\\') {
        lstrcatW(searchPath, L"\\");
    }
    lstrcatW(searchPath, L"*");

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }

    do {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        WCHAR childPath[MAX_PATH];
        lstrcpyW(childPath, path);
        int clen = lstrlenW(childPath);
        if (clen > 0 && childPath[clen - 1] != L'\\') {
            lstrcatW(childPath, L"\\");
        }
        lstrcatW(childPath, findData.cFileName);

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            total += computeDirectorySize(childPath);
        } else {
            ULARGE_INTEGER fileSize;
            fileSize.LowPart = findData.nFileSizeLow;
            fileSize.HighPart = findData.nFileSizeHigh;
            total += fileSize.QuadPart;
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
    return total;
}

// 缓存文件路径
static void getDirSizeCachePath(WCHAR* outPath, int bufferSize) {
    getExecutableDirectory(outPath, bufferSize);
    lstrcatW(outPath, L"dir_sizes.txt");
}

// 从本地缓存读取目录大小
BOOL getCachedDirSize(const WCHAR* path, ULONGLONG* sizeOut) {
    if (loadSqliteDll()) {
        sqlite3* db = NULL;
        if (sqliteOpenDb(&db)) {
            sqliteEnsureTable(db);
            const char* sql = "SELECT size FROM dir_sizes WHERE path=?1";
            sqlite3_stmt* stmt = NULL;
            if (p_sqlite3_prepare_v2 && p_sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == 0) {
                std::string pathUtf8 = WideToUtf8(path);
                if (p_sqlite3_bind_text) p_sqlite3_bind_text(stmt, 1, pathUtf8.c_str(), (int)pathUtf8.size(), NULL);
                int rc = p_sqlite3_step ? p_sqlite3_step(stmt) : 1;
                if (rc == 100) {
                    long long v = p_sqlite3_column_int64 ? p_sqlite3_column_int64(stmt, 0) : 0;
                    *sizeOut = (ULONGLONG)v;
                    if (p_sqlite3_finalize) p_sqlite3_finalize(stmt);
                    if (p_sqlite3_close) p_sqlite3_close(db);
                    return TRUE;
                }
                if (p_sqlite3_finalize) p_sqlite3_finalize(stmt);
            }
            if (p_sqlite3_close) p_sqlite3_close(db);
        }
    }

    WCHAR cachePath[MAX_PATH];
    getDirSizeCachePath(cachePath, MAX_PATH);
    FILE* fp = NULL;
    errno_t err = _wfopen_s(&fp, cachePath, L"r, ccs=UTF-8");
    if (err != 0 || !fp) {
        return FALSE;
    }
    WCHAR line[4096];
    while (fgetws(line, 4096, fp)) {
        int len = lstrlenW(line);
        while (len > 0 && (line[len-1] == L'\n' || line[len-1] == L'\r')) {
            line[--len] = L'\0';
        }
        WCHAR* tab = wcschr(line, L'\t');
        if (!tab) continue;
        *tab = L'\0';
        const WCHAR* key = line;
        const WCHAR* valStr = tab + 1;
        if (lstrcmpiW(key, path) == 0) {
            ULONGLONG val = 0;
            swscanf_s(valStr, L"%llu", &val);
            *sizeOut = val;
            fclose(fp);
            return TRUE;
        }
    }
    fclose(fp);
    return FALSE;
}

// 写入本地缓存目录大小
void setCachedDirSize(const WCHAR* path, ULONGLONG size) {
    if (loadSqliteDll()) {
        sqlite3* db = NULL;
        if (sqliteOpenDb(&db)) {
            sqliteEnsureTable(db);
            const char* sql = "REPLACE INTO dir_sizes(path,size,updated_at) VALUES(?1,?2,datetime('now'))";
            sqlite3_stmt* stmt = NULL;
            if (p_sqlite3_prepare_v2 && p_sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == 0) {
                std::string pathUtf8 = WideToUtf8(path);
                if (p_sqlite3_bind_text) p_sqlite3_bind_text(stmt, 1, pathUtf8.c_str(), (int)pathUtf8.size(), NULL);
                if (p_sqlite3_bind_int64) p_sqlite3_bind_int64(stmt, 2, (long long)size);
                if (p_sqlite3_step) p_sqlite3_step(stmt);
                if (p_sqlite3_finalize) p_sqlite3_finalize(stmt);
            }
            if (p_sqlite3_close) p_sqlite3_close(db);
            return;
        }
    }

    WCHAR cachePath[MAX_PATH];
    getDirSizeCachePath(cachePath, MAX_PATH);
    FILE* fp = NULL;
    errno_t err = _wfopen_s(&fp, cachePath, L"a, ccs=UTF-8");
    if (err != 0 || !fp) {
        LogMessage(L"[ERROR] 无法写入缓存文件: %s", cachePath);
        return;
    }
    fwprintf(fp, L"%s\t%llu\n", path, size);
    fclose(fp);
}
