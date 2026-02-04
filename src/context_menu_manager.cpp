#include "context_menu_manager.h"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

#define ID_LIST_VIEW 3001
#define ID_BTN_ADD 3002
#define ID_BTN_DELETE 3003

// Add Dialog Controls
#define ID_EDIT_NAME 4001
#define ID_EDIT_COMMAND 4002
#define ID_COMBO_TYPE 4003
#define ID_BTN_OK 4004
#define ID_BTN_CANCEL 4005

struct ContextMenuItem {
    std::wstring name;
    std::wstring command;
    std::wstring regPath;
    std::wstring keyName;
    std::wstring typeDisplay;
    HKEY hRoot;
};

static std::vector<ContextMenuItem> g_menuItems;
static HWND g_hListView = NULL;
static HWND g_hBtnAdd = NULL;
static HWND g_hBtnDelete = NULL;
static BOOL g_isVisible = FALSE;

// Helper to read registry keys
void LoadRegistryItems(HKEY hRoot, const std::wstring& path, const std::wstring& typeDisplay) {
    HKEY hKey;
    if (RegOpenKeyExW(hRoot, path.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return;
    }

    WCHAR keyName[256];
    DWORD index = 0;
    DWORD len = 256;

    while (RegEnumKeyExW(hKey, index, keyName, &len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        // Check if it has a "command" subkey
        std::wstring itemPath = path + L"\\" + keyName;
        std::wstring commandPath = itemPath + L"\\command";
        
        HKEY hCommandKey;
        if (RegOpenKeyExW(hRoot, commandPath.c_str(), 0, KEY_READ, &hCommandKey) == ERROR_SUCCESS) {
            WCHAR command[MAX_PATH * 2];
            DWORD dataSize = sizeof(command);
            if (RegQueryValueExW(hCommandKey, NULL, NULL, NULL, (LPBYTE)command, &dataSize) == ERROR_SUCCESS) {
                ContextMenuItem item;
                item.name = keyName; 
                
                // Check if there is a separate display name
                WCHAR displayName[256];
                DWORD dispLen = sizeof(displayName);
                if (RegQueryValueExW(hKey, NULL, NULL, NULL, (LPBYTE)displayName, &dispLen) == ERROR_SUCCESS && displayName[0] != 0) {
                   item.name = displayName;
                   item.keyName = keyName;
                } else {
                   item.keyName = keyName;
                }

                item.command = command;
                item.regPath = path;
                item.typeDisplay = typeDisplay;
                item.hRoot = hRoot;
                g_menuItems.push_back(item);
            }
            RegCloseKey(hCommandKey);
        }
        
        index++;
        len = 256;
    }
    RegCloseKey(hKey);
}

// Helper to read shell extensions
void LoadShellExtensions(HKEY hRoot, const std::wstring& path, const std::wstring& typeDisplay) {
    HKEY hKey;
    if (RegOpenKeyExW(hRoot, path.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return;
    }

    WCHAR keyName[256];
    DWORD index = 0;
    DWORD len = 256;

    while (RegEnumKeyExW(hKey, index, keyName, &len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        ContextMenuItem item;
        item.name = keyName;
        item.keyName = keyName;
        item.regPath = path;
        item.typeDisplay = typeDisplay + L" [ShellEx]";
        item.hRoot = hRoot;
        
        // Try to find the CLSID
        std::wstring targetCLSID;
        std::wstring rawValue;

        HKEY hSubKey;
        if (RegOpenKeyExW(hKey, keyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
             WCHAR buf[256];
             DWORD bufSize = sizeof(buf);
             if (RegQueryValueExW(hSubKey, NULL, NULL, NULL, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
                 rawValue = buf;
             }
             RegCloseKey(hSubKey);
        }

        if (rawValue.length() > 0 && rawValue[0] == L'{') {
            targetCLSID = rawValue;
        } else if (std::wstring(keyName).length() > 0 && keyName[0] == L'{') {
            targetCLSID = keyName;
        }

        // If we found a CLSID, resolve it to Description and DLL
        if (!targetCLSID.empty()) {
             std::wstring description;
             std::wstring dllPath;
             
             HKEY hClsidKey;
             std::wstring clsidPath = L"CLSID\\" + targetCLSID;
             if (RegOpenKeyExW(HKEY_CLASSES_ROOT, clsidPath.c_str(), 0, KEY_READ, &hClsidKey) == ERROR_SUCCESS) {
                 WCHAR descBuf[256];
                 DWORD descSize = sizeof(descBuf);
                 if (RegQueryValueExW(hClsidKey, NULL, NULL, NULL, (LPBYTE)descBuf, &descSize) == ERROR_SUCCESS) {
                     description = descBuf;
                 }
                 
                 HKEY hInProc;
                 if (RegOpenKeyExW(hClsidKey, L"InProcServer32", 0, KEY_READ, &hInProc) == ERROR_SUCCESS) {
                     WCHAR pathBuf[MAX_PATH];
                     DWORD pathSize = sizeof(pathBuf);
                     if (RegQueryValueExW(hInProc, NULL, NULL, NULL, (LPBYTE)pathBuf, &pathSize) == ERROR_SUCCESS) {
                         dllPath = pathBuf;
                     }
                     RegCloseKey(hInProc);
                 }
                 RegCloseKey(hClsidKey);
             }

             std::wstring info = L"CLSID: " + targetCLSID;
             if (!description.empty()) info += L" (" + description + L")";
             if (!dllPath.empty()) info += L" -> " + dllPath;
             
             item.command = info;
        } else {
             item.command = rawValue.empty() ? L"(Shell Extension)" : rawValue;
        }

        g_menuItems.push_back(item);
        
        index++;
        len = 256;
    }
    RegCloseKey(hKey);
}

void RefreshContextMenuList() {
    g_menuItems.clear();
    ListView_DeleteAllItems(g_hListView);

    // Static Verbs (User)
    LoadRegistryItems(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell", L"所有文件");
    LoadRegistryItems(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell", L"文件夹");
    LoadRegistryItems(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\Background\\shell", L"文件夹背景");
    LoadRegistryItems(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell", L"驱动器");

    // Static Verbs (System)
    LoadRegistryItems(HKEY_CLASSES_ROOT, L"*\\shell", L"所有文件(System)");
    LoadRegistryItems(HKEY_CLASSES_ROOT, L"Directory\\shell", L"文件夹(System)");
    LoadRegistryItems(HKEY_CLASSES_ROOT, L"Directory\\Background\\shell", L"文件夹背景(System)");
    LoadRegistryItems(HKEY_CLASSES_ROOT, L"Drive\\shell", L"驱动器(System)");
    LoadRegistryItems(HKEY_CLASSES_ROOT, L"AllFilesystemObjects\\shell", L"文件系统对象(System)");

    // Shell Extensions (System - usually in HKCR)
    LoadShellExtensions(HKEY_CLASSES_ROOT, L"*\\shellex\\ContextMenuHandlers", L"所有文件");
    LoadShellExtensions(HKEY_CLASSES_ROOT, L"Directory\\shellex\\ContextMenuHandlers", L"文件夹");
    LoadShellExtensions(HKEY_CLASSES_ROOT, L"Directory\\Background\\shellex\\ContextMenuHandlers", L"文件夹背景");
    LoadShellExtensions(HKEY_CLASSES_ROOT, L"Drive\\shellex\\ContextMenuHandlers", L"驱动器");
    LoadShellExtensions(HKEY_CLASSES_ROOT, L"AllFilesystemObjects\\shellex\\ContextMenuHandlers", L"文件系统对象");

    for (size_t i = 0; i < g_menuItems.size(); ++i) {
        const auto& item = g_menuItems[i];
        
        LVITEMW lvi = {0};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)item.name.c_str();
        lvi.lParam = (LPARAM)i; // Store index
        
        ListView_InsertItem(g_hListView, &lvi);
        
        std::wstring source = (item.hRoot == HKEY_CURRENT_USER) ? L"[用户]" : L"[系统]";
        ListView_SetItemText(g_hListView, (int)i, 1, (LPWSTR)source.c_str());
        ListView_SetItemText(g_hListView, (int)i, 2, (LPWSTR)item.typeDisplay.c_str());
        ListView_SetItemText(g_hListView, (int)i, 3, (LPWSTR)item.command.c_str());
    }
}

// Dialog Procedure for Add Item
LRESULT CALLBACK AddDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            CreateWindowW(L"STATIC", L"名称:", WS_CHILD | WS_VISIBLE, 10, 10, 50, 20, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 70, 10, 200, 20, hwnd, (HMENU)ID_EDIT_NAME, NULL, NULL);

            CreateWindowW(L"STATIC", L"命令:", WS_CHILD | WS_VISIBLE, 10, 40, 50, 20, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 70, 40, 200, 20, hwnd, (HMENU)ID_EDIT_COMMAND, NULL, NULL);

            CreateWindowW(L"STATIC", L"类型:", WS_CHILD | WS_VISIBLE, 10, 70, 50, 20, hwnd, NULL, NULL, NULL);
            HWND hCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 70, 70, 200, 100, hwnd, (HMENU)ID_COMBO_TYPE, NULL, NULL);
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"所有文件");
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"文件夹");
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"文件夹背景");
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"驱动器");
            SendMessage(hCombo, CB_SETCURSEL, 0, 0);

            CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 50, 110, 80, 25, hwnd, (HMENU)ID_BTN_OK, NULL, NULL);
            CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 150, 110, 80, 25, hwnd, (HMENU)ID_BTN_CANCEL, NULL, NULL);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_BTN_OK) {
                WCHAR name[256], command[MAX_PATH * 2];
                GetDlgItemTextW(hwnd, ID_EDIT_NAME, name, 256);
                GetDlgItemTextW(hwnd, ID_EDIT_COMMAND, command, MAX_PATH * 2);
                HWND hCombo = GetDlgItem(hwnd, ID_COMBO_TYPE);
                int idx = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
                
                if (lstrlenW(name) == 0 || lstrlenW(command) == 0) {
                    MessageBoxW(hwnd, L"名称和命令不能为空", L"错误", MB_OK | MB_ICONERROR);
                    return 0;
                }

                std::wstring regPath;
                if (idx == 0) regPath = L"Software\\Classes\\*\\shell";
                else if (idx == 1) regPath = L"Software\\Classes\\Directory\\shell";
                else if (idx == 2) regPath = L"Software\\Classes\\Directory\\Background\\shell";
                else regPath = L"Software\\Classes\\Drive\\shell";

                // Create Registry Keys
                HKEY hKey;
                std::wstring fullPath = regPath + L"\\" + name;
                if (RegCreateKeyExW(HKEY_CURRENT_USER, fullPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
                    // Set Command
                    HKEY hCmdKey;
                    std::wstring cmdPath = L"command";
                    if (RegCreateKeyExW(hKey, cmdPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hCmdKey, NULL) == ERROR_SUCCESS) {
                        RegSetValueExW(hCmdKey, NULL, 0, REG_SZ, (const BYTE*)command, (lstrlenW(command) + 1) * sizeof(WCHAR));
                        RegCloseKey(hCmdKey);
                        MessageBoxW(hwnd, L"添加成功", L"成功", MB_OK | MB_ICONINFORMATION);
                        DestroyWindow(hwnd);
                    } else {
                         MessageBoxW(hwnd, L"创建Command键失败", L"错误", MB_OK | MB_ICONERROR);
                    }
                    RegCloseKey(hKey);
                } else {
                    MessageBoxW(hwnd, L"创建注册表键失败", L"错误", MB_OK | MB_ICONERROR);
                }
            } else if (id == ID_BTN_CANCEL) {
                DestroyWindow(hwnd);
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void InitContextMenuManager(HWND parent) {
    // Initialize ListView
    g_hListView = CreateWindowW(WC_LISTVIEW, L"", 
        WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | WS_HSCROLL | WS_VSCROLL,
        0, 0, 0, 0, parent, (HMENU)ID_LIST_VIEW, GetModuleHandle(NULL), NULL);
    
    ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    // Add Columns
    LVCOLUMNW col = {0};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    
    col.pszText = (LPWSTR)L"名称";
    col.cx = 150;
    ListView_InsertColumn(g_hListView, 0, &col);
    
    col.pszText = (LPWSTR)L"来源";
    col.cx = 80;
    ListView_InsertColumn(g_hListView, 1, &col);

    col.pszText = (LPWSTR)L"类型";
    col.cx = 150;
    ListView_InsertColumn(g_hListView, 2, &col);

    col.pszText = (LPWSTR)L"命令/CLSID";
    col.cx = 400;
    ListView_InsertColumn(g_hListView, 3, &col);

    // Initialize Buttons
    g_hBtnAdd = CreateWindowW(L"BUTTON", L"添加项", WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, parent, (HMENU)ID_BTN_ADD, GetModuleHandle(NULL), NULL);
    g_hBtnDelete = CreateWindowW(L"BUTTON", L"删除项", WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, parent, (HMENU)ID_BTN_DELETE, GetModuleHandle(NULL), NULL);

    RefreshContextMenuList();
}

void ShowContextMenuManager(BOOL show) {
    g_isVisible = show;
    int cmd = show ? SW_SHOW : SW_HIDE;
    ShowWindow(g_hListView, cmd);
    ShowWindow(g_hBtnAdd, cmd);
    ShowWindow(g_hBtnDelete, cmd);
}

void ResizeContextMenuManager(RECT rect) {
    if (!g_hListView) return;
    
    int btnHeight = 30;
    int padding = 10;
    
    // Buttons at the bottom
    int btnY = rect.bottom - btnHeight - padding;
    MoveWindow(g_hBtnAdd, rect.left + padding, btnY, 80, btnHeight, TRUE);
    MoveWindow(g_hBtnDelete, rect.left + padding + 80 + padding, btnY, 80, btnHeight, TRUE);
    
    // ListView takes remaining space
    int listHeight = btnY - padding - rect.top;
    if (listHeight < 0) listHeight = 0;

    MoveWindow(g_hListView, rect.left, rect.top, rect.right - rect.left, listHeight, TRUE);
}

BOOL HandleContextMenuManagerMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!g_isVisible) return FALSE;
    
    if (uMsg == WM_COMMAND) {
        int id = LOWORD(wParam);
        if (id == ID_BTN_ADD) {
            WNDCLASSEXW wc = {0};
            wc.cbSize = sizeof(WNDCLASSEX);
            wc.lpfnWndProc = AddDialogProc;
            wc.hInstance = GetModuleHandle(NULL);
            wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wc.lpszClassName = L"AddContextItemClass";
            RegisterClassExW(&wc);

            HWND hAdd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"AddContextItemClass", L"添加右键菜单项",
                WS_VISIBLE | WS_SYSMENU | WS_CAPTION,
                CW_USEDEFAULT, CW_USEDEFAULT, 300, 200,
                hwnd, NULL, GetModuleHandle(NULL), NULL);
            
            EnableWindow(hwnd, FALSE);
            MSG msg;
            while (IsWindow(hAdd) && GetMessage(&msg, NULL, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            EnableWindow(hwnd, TRUE);
            SetForegroundWindow(hwnd);
            RefreshContextMenuList();
            return TRUE;
        } else if (id == ID_BTN_DELETE) {
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (sel != -1) {
                LVITEMW lvi = {0};
                lvi.mask = LVIF_PARAM;
                lvi.iItem = sel;
                ListView_GetItem(g_hListView, &lvi);
                size_t index = (size_t)lvi.lParam;

                if (index < g_menuItems.size()) {
                    const auto& item = g_menuItems[index];
                    
                    std::wstring msg = L"确定要删除 \"" + item.name + L"\" 吗？\n";
                    if (item.hRoot == HKEY_CLASSES_ROOT) {
                        msg += L"\n注意：这是系统或全局注册项，删除可能需要管理员权限。";
                    }

                    if (MessageBoxW(hwnd, msg.c_str(), L"确认删除", MB_YESNO | MB_ICONWARNING) == IDYES) {
                        std::wstring fullPath = item.regPath + L"\\" + item.keyName;
                        LONG res = RegDeleteTreeW(item.hRoot, fullPath.c_str());
                        if (res == ERROR_SUCCESS) {
                            RefreshContextMenuList();
                        } else {
                            WCHAR errorMsg[256];
                            swprintf_s(errorMsg, 256, L"删除失败 (错误代码: %d)。\n如果这是系统项，请尝试以管理员身份运行本程序。", res);
                            MessageBoxW(hwnd, errorMsg, L"错误", MB_OK | MB_ICONERROR);
                        }
                    }
                }
            } else {
                MessageBoxW(hwnd, L"请先选择要删除的项目", L"提示", MB_OK | MB_ICONINFORMATION);
            }
            return TRUE;
        }
    }
    
    return FALSE;
}
