#include "registry_integration.h"
#include <string>
#include <shlwapi.h>
#include <shlobj.h>
#include <stdio.h>

#pragma comment(lib, "shlwapi.lib")

// 获取当前可执行文件路径
std::wstring GetCurrentExePath() {
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return std::wstring(path);
}

// 设置注册表键值
BOOL SetRegistryKey(HKEY hRootKey, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& value) {
    HKEY hKey;
    LONG result = RegCreateKeyExW(hRootKey, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }

    result = RegSetValueExW(hKey, valueName.c_str(), 0, REG_SZ, (const BYTE*)value.c_str(), (value.length() + 1) * sizeof(WCHAR));
    RegCloseKey(hKey);

    return (result == ERROR_SUCCESS);
}

// 删除注册表键
BOOL DeleteRegistryKey(HKEY hRootKey, const std::wstring& subKey) {
    // RegDeleteTree is safer for deleting keys with subkeys
    LONG result = RegDeleteTreeW(hRootKey, subKey.c_str());
    // If the key doesn't exist, we consider it a success (state is as desired)
    if (result == ERROR_FILE_NOT_FOUND) return TRUE;
    return (result == ERROR_SUCCESS);
}

// 检查注册表值
BOOL CheckRegistryValue(HKEY hRootKey, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& expectedValue) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(hRootKey, subKey.c_str(), 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }

    WCHAR buffer[MAX_PATH * 2];
    DWORD dataSize = sizeof(buffer);
    result = RegQueryValueExW(hKey, valueName.c_str(), NULL, NULL, (LPBYTE)buffer, &dataSize);
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        return FALSE;
    }

    return (expectedValue == buffer);
}

BOOL SetAsDefaultFileManager() {
    std::wstring exePath = GetCurrentExePath();
    std::wstring command = L"\"" + exePath + L"\" \"%1\"";

    // 设置 Directory 的打开命令
    if (!SetRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open\\command", L"", command)) {
        return FALSE;
    }

    // 设置 Drive 的打开命令
    if (!SetRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open\\command", L"", command)) {
        return FALSE;
    }

    // 通知系统关联改变
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return TRUE;
}

BOOL RestoreDefaultFileManager() {
    BOOL success = TRUE;

    // 删除 Directory 的自定义打开命令，恢复系统默认
    // 注意：我们只删除 open\command 键，这样如果还有其他 shell 扩展不受影响
    // 但如果用户本来就没有 HKCU 下的这些键（通常如此），删除整个 shell\open 也是安全的
    // 为了安全起见，我们删除我们创建的 command 键
    
    // 删除 Directory\shell\open\command
    if (!DeleteRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open\\command")) {
        success = FALSE;
    }
    // 尝试删除空的 open 键（可选）
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open");

    // 删除 Drive\shell\open\command
    if (!DeleteRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open\\command")) {
        success = FALSE;
    }
    // 尝试删除空的 open 键（可选）
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open");

    // 通知系统关联改变
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return success;
}

BOOL IsDefaultFileManager() {
    std::wstring exePath = GetCurrentExePath();
    std::wstring command = L"\"" + exePath + L"\" \"%1\"";

    if (!CheckRegistryValue(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open\\command", L"", command)) {
        return FALSE;
    }

    if (!CheckRegistryValue(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open\\command", L"", command)) {
        return FALSE;
    }

    return TRUE;
}
