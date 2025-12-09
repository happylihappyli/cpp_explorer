#include "log.h"
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <stdarg.h>

void LogMessage(const WCHAR* format, ...) {
    va_list args;
    va_start(args, format);
    
    WCHAR buffer[1024];
    vswprintf_s(buffer, 1024, format, args);
    
    // 输出到控制台（使用立即刷新的方式）
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != NULL && hConsole != INVALID_HANDLE_VALUE) {
        DWORD charsWritten;
        WCHAR consoleBuffer[2048];
        swprintf_s(consoleBuffer, 2048, L"[LOG] %s\n", buffer);
        WriteConsoleW(hConsole, consoleBuffer, wcslen(consoleBuffer), &charsWritten, NULL);
        FlushFileBuffers(hConsole);
    }
    
    // 同时输出到标准输出流（用于重定向捕获）
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut != NULL && hStdOut != INVALID_HANDLE_VALUE) {
        DWORD charsWritten;
        
        // 转换为ASCII/UTF-8兼容格式
        char asciiBuffer[4096];
        int asciiLen = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, asciiBuffer, 4096, NULL, NULL);
        if (asciiLen > 0) {
            char stdBuffer[4096];
            snprintf(stdBuffer, 4096, "[LOG] %s\n", asciiBuffer);
            WriteFile(hStdOut, stdBuffer, strlen(stdBuffer), &charsWritten, NULL);
            FlushFileBuffers(hStdOut);
        }
    }
    
    // 同时输出到调试器
    OutputDebugStringW(L"[LOG] ");
    OutputDebugStringW(buffer);
    OutputDebugStringW(L"\n");
    
    va_end(args);
}