#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <windows.h>

// 文件信息结构
typedef struct {
    WCHAR name[MAX_PATH];
    BOOL isDirectory;
    ULONGLONG size;
    FILETIME creationTime;
    FILETIME lastWriteTime;
} FileInfo;

// 列出目录内容
int listDirectory(const WCHAR* path, FileInfo* files, int maxFiles);

// 格式化文件大小
void formatFileSize(ULONGLONG size, WCHAR* buffer, int bufferSize);
void formatFileTime(const FILETIME* ft, WCHAR* buffer, int bufferSize);

// 获取可执行文件所在目录
void getExecutableDirectory(WCHAR* buffer, int bufferSize);

// 计算目录累计大小（递归）
ULONGLONG computeDirectorySize(const WCHAR* path, BOOL* isComplete = NULL);

// 从本地缓存读取目录大小
BOOL getCachedDirSize(const WCHAR* path, ULONGLONG* sizeOut);

// 写入本地缓存目录大小
void setCachedDirSize(const WCHAR* path, ULONGLONG size);

// 删除文件或目录到回收站
bool DeleteToRecycleBin(const WCHAR* path);

#endif // FILE_UTILS_H
