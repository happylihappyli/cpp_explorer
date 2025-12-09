#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <windows.h>

// 文件信息结构
typedef struct {
    WCHAR name[MAX_PATH];
    BOOL isDirectory;
    ULONGLONG size;
} FileInfo;

// 列出目录内容
int listDirectory(const WCHAR* path, FileInfo* files, int maxFiles);

// 格式化文件大小
void formatFileSize(ULONGLONG size, WCHAR* buffer, int bufferSize);

#endif // FILE_UTILS_H