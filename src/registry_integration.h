#ifndef REGISTRY_INTEGRATION_H
#define REGISTRY_INTEGRATION_H

#include <windows.h>

// 设置为默认文件管理器
BOOL SetAsDefaultFileManager();

// 恢复系统默认文件管理器
BOOL RestoreDefaultFileManager();

// 检查是否已经是默认文件管理器
BOOL IsDefaultFileManager();

#endif // REGISTRY_INTEGRATION_H
