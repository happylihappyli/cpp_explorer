#ifndef LOG_H
#define LOG_H

#include <windows.h>
#include <stdio.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

void LogMessage(const WCHAR* format, ...);

#ifdef __cplusplus
}
#endif

#endif // LOG_H