#include "file_utils.h"
#include "log.h"
#include <windows.h>
#include <shlwapi.h>

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