#include "settings.h"
#include "file_utils.h"
#include "log.h"
#include "language.h"
#include <commdlg.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <sstream>

static WCHAR g_editorPath[MAX_PATH] = L"D:\\Office\\Notepad_abc\\notepad_abc.exe";
static int g_fontSize = 9;  // 默认字体大小
static std::vector<FileAssociation> g_associations;
static HWND g_hDialog = NULL;
static HWND g_hEditEditor = NULL;
static HWND g_hListAssoc = NULL;
static HWND g_hEditExt = NULL;
static HWND g_hEditName = NULL;
static HWND g_hEditPath = NULL;
static HWND g_hEditFontSize = NULL;  // 字体大小编辑框

void getSettingsFilePath(WCHAR* buffer, int bufferSize) {
    getExecutableDirectory(buffer, bufferSize);
    lstrcatW(buffer, L"settings.ini");
}

void loadSettings() {
    WCHAR path[MAX_PATH];
    getSettingsFilePath(path, MAX_PATH);
    
    // Load editor path
    GetPrivateProfileStringW(L"Settings", L"EditorPath", L"D:\\Office\\Notepad_abc\\notepad_abc.exe", g_editorPath, MAX_PATH, path);
    
    // Load font size
    g_fontSize = GetPrivateProfileIntW(L"Settings", L"FontSize", 9, path);
    if (g_fontSize < 6) g_fontSize = 6;
    if (g_fontSize > 24) g_fontSize = 24;

    // Load associations
    g_associations.clear();
    // 32KB buffer for section
    std::vector<WCHAR> buffer(32768);
    GetPrivateProfileSectionW(L"Associations", buffer.data(), 32768, path);

    WCHAR* p = buffer.data();
    while (*p) {
        std::wstring line = p;
        size_t eqPos = line.find(L'=');
        if (eqPos != std::wstring::npos) {
            std::wstring ext = line.substr(0, eqPos);
            std::wstring val = line.substr(eqPos + 1);
            
            size_t pipePos = val.find(L'|');
            if (pipePos != std::wstring::npos) {
                std::wstring name = val.substr(0, pipePos);
                std::wstring cmd = val.substr(pipePos + 1);
                g_associations.push_back({ext, name, cmd});
            }
        }
        p += lstrlenW(p) + 1;
    }
}

void saveSettings() {
    WCHAR path[MAX_PATH];
    getSettingsFilePath(path, MAX_PATH);
    
    // Save editor path
    WritePrivateProfileStringW(L"Settings", L"EditorPath", g_editorPath, path);
    
    // Save font size
    WCHAR fontSizeStr[16];
    wsprintfW(fontSizeStr, L"%d", g_fontSize);
    WritePrivateProfileStringW(L"Settings", L"FontSize", fontSizeStr, path);

    // Save associations
    // Clear section first
    WritePrivateProfileStringW(L"Associations", NULL, NULL, path);
    
    for (const auto& assoc : g_associations) {
        std::wstring val = assoc.name + L"|" + assoc.command;
        WritePrivateProfileStringW(L"Associations", assoc.ext.c_str(), val.c_str(), path);
    }
}

void getEditorPath(WCHAR* buffer, int bufferSize) {
    lstrcpyW(buffer, g_editorPath);
}

void setEditorPath(const WCHAR* path) {
    lstrcpyW(g_editorPath, path);
    saveSettings();
}

int getFontSize() {
    return g_fontSize;
}

void setFontSize(int size) {
    if (size < 6) size = 6;
    if (size > 24) size = 24;
    g_fontSize = size;
    saveSettings();
}

std::vector<FileAssociation> getFileAssociations() {
    return g_associations;
}

void addFileAssociation(const std::wstring& ext, const std::wstring& name, const std::wstring& command) {
    // Check if exists, update if so
    for (auto& assoc : g_associations) {
        if (lstrcmpiW(assoc.ext.c_str(), ext.c_str()) == 0) {
            assoc.name = name;
            assoc.command = command;
            saveSettings();
            return;
        }
    }
    g_associations.push_back({ext, name, command});
    saveSettings();
}

void removeFileAssociation(int index) {
    if (index >= 0 && index < (int)g_associations.size()) {
        g_associations.erase(g_associations.begin() + index);
        saveSettings();
    }
}

// Dialog handling
#define ID_EDIT_EDITOR 1001
#define ID_BTN_BROWSE_EDITOR 1002
#define ID_LIST_ASSOC 1003
#define ID_EDIT_EXT 1004
#define ID_EDIT_NAME 1005
#define ID_EDIT_PATH 1006
#define ID_BTN_BROWSE_PATH 1007
#define ID_BTN_ADD 1008
#define ID_BTN_REMOVE 1009
#define ID_BTN_SAVE 1010
#define ID_BTN_CANCEL 1011
#define ID_EDIT_FONTSIZE 1012  // 字体大小编辑框
#define ID_COMBO_LANGUAGE 1013  // 语言选择下拉框

static HWND g_hListLanguage = NULL;  // 语言选择列表框

void RefreshList() {
    SendMessage(g_hListAssoc, LB_RESETCONTENT, 0, 0);
    for (const auto& assoc : g_associations) {
        WCHAR buf[1024];
        wsprintfW(buf, L"%s - %s (%s)", assoc.ext.c_str(), assoc.name.c_str(), assoc.command.c_str());
        SendMessageW(g_hListAssoc, LB_ADDSTRING, 0, (LPARAM)buf);
    }
}

LRESULT CALLBACK SettingsDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

            // Language Selection
            CreateWindowW(L"STATIC", getText(TEXT_SETTINGS_LANGUAGE), WS_CHILD | WS_VISIBLE, 10, 10, 150, 20, hwnd, NULL, NULL, NULL);
            g_hListLanguage = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL, 10, 30, 200, 200, hwnd, (HMENU)ID_COMBO_LANGUAGE, NULL, NULL);
            SendMessageW(g_hListLanguage, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(g_hListLanguage, CB_ADDSTRING, 0, (LPARAM)L"简体中文");
            SendMessageW(g_hListLanguage, CB_ADDSTRING, 0, (LPARAM)L"English");
            SendMessageW(g_hListLanguage, CB_SETCURSEL, (WPARAM)getCurrentLanguage(), 0);

            // Default Editor
            CreateWindowW(L"STATIC", L"默认编辑器路径:", WS_CHILD | WS_VISIBLE, 250, 10, 120, 20, hwnd, NULL, NULL, NULL);
            g_hEditEditor = CreateWindowW(L"EDIT", g_editorPath, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 250, 30, 200, 25, hwnd, (HMENU)ID_EDIT_EDITOR, NULL, NULL);
            SendMessage(g_hEditEditor, WM_SETFONT, (WPARAM)hFont, TRUE);
            CreateWindowW(L"BUTTON", L"浏览...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 460, 30, 80, 25, hwnd, (HMENU)ID_BTN_BROWSE_EDITOR, NULL, NULL);

            // Font Size
            CreateWindowW(L"STATIC", getText(TEXT_SETTINGS_FONT_SIZE), WS_CHILD | WS_VISIBLE, 10, 65, 120, 20, hwnd, NULL, NULL, NULL);
            WCHAR fontSizeStr[16];
            wsprintfW(fontSizeStr, L"%d", g_fontSize);
            g_hEditFontSize = CreateWindowW(L"EDIT", fontSizeStr, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 10, 85, 80, 25, hwnd, (HMENU)ID_EDIT_FONTSIZE, NULL, NULL);
            SendMessage(g_hEditFontSize, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Associations List
            CreateWindowW(L"STATIC", getText(TEXT_SETTINGS_ASSOC), WS_CHILD | WS_VISIBLE, 10, 130, 120, 20, hwnd, NULL, NULL, NULL);
            g_hListAssoc = CreateWindowW(L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 10, 150, 530, 200, hwnd, (HMENU)ID_LIST_ASSOC, NULL, NULL);
            SendMessage(g_hListAssoc, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Add/Edit Controls
            CreateWindowW(L"STATIC", getText(TEXT_SETTINGS_EXT), WS_CHILD | WS_VISIBLE, 10, 365, 60, 20, hwnd, NULL, NULL, NULL);
            g_hEditExt = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 70, 365, 60, 25, hwnd, (HMENU)ID_EDIT_EXT, NULL, NULL);
            SendMessage(g_hEditExt, WM_SETFONT, (WPARAM)hFont, TRUE);

            CreateWindowW(L"STATIC", getText(TEXT_SETTINGS_NAME), WS_CHILD | WS_VISIBLE, 140, 365, 40, 20, hwnd, NULL, NULL, NULL);
            g_hEditName = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 180, 365, 100, 25, hwnd, (HMENU)ID_EDIT_NAME, NULL, NULL);
            SendMessage(g_hEditName, WM_SETFONT, (WPARAM)hFont, TRUE);

            CreateWindowW(L"STATIC", getText(TEXT_SETTINGS_PROGRAM_PATH), WS_CHILD | WS_VISIBLE, 10, 400, 60, 20, hwnd, NULL, NULL, NULL);
            g_hEditPath = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 70, 400, 310, 25, hwnd, (HMENU)ID_EDIT_PATH, NULL, NULL);
            SendMessage(g_hEditPath, WM_SETFONT, (WPARAM)hFont, TRUE);
            CreateWindowW(L"BUTTON", getText(TEXT_SETTINGS_BROWSE), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 390, 400, 80, 25, hwnd, (HMENU)ID_BTN_BROWSE_PATH, NULL, NULL);

            // Action Buttons
            CreateWindowW(L"BUTTON", getText(TEXT_SETTINGS_ADD_UPDATE), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 440, 100, 25, hwnd, (HMENU)ID_BTN_ADD, NULL, NULL);
            CreateWindowW(L"BUTTON", getText(TEXT_SETTINGS_REMOVE), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 120, 440, 100, 25, hwnd, (HMENU)ID_BTN_REMOVE, NULL, NULL);

            // Dialog Buttons
            CreateWindowW(L"BUTTON", getText(TEXT_SETTINGS_SAVE_ALL), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 150, 485, 80, 25, hwnd, (HMENU)ID_BTN_SAVE, NULL, NULL);
            CreateWindowW(L"BUTTON", getText(TEXT_SETTINGS_CANCEL), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 250, 485, 80, 25, hwnd, (HMENU)ID_BTN_CANCEL, NULL, NULL);

            RefreshList();
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_BTN_BROWSE_EDITOR) {
                WCHAR buffer[MAX_PATH] = {0};
                GetWindowTextW(g_hEditEditor, buffer, MAX_PATH);
                OPENFILENAMEW ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFile = buffer;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Executables (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) {
                    SetWindowTextW(g_hEditEditor, buffer);
                }
            } else if (id == ID_BTN_BROWSE_PATH) {
                WCHAR buffer[MAX_PATH] = {0};
                GetWindowTextW(g_hEditPath, buffer, MAX_PATH);
                OPENFILENAMEW ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFile = buffer;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Executables (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) {
                    SetWindowTextW(g_hEditPath, buffer);
                }
            } else if (id == ID_BTN_ADD) {
                WCHAR ext[64], name[64], path[MAX_PATH];
                GetWindowTextW(g_hEditExt, ext, 64);
                GetWindowTextW(g_hEditName, name, 64);
                GetWindowTextW(g_hEditPath, path, MAX_PATH);

                if (lstrlenW(ext) > 0 && lstrlenW(name) > 0 && lstrlenW(path) > 0) {
                    std::wstring extStr = ext;
                    if (extStr.length() > 0 && extStr[0] != L'.') {
                        extStr = L"." + extStr;
                    }
                    addFileAssociation(extStr, name, path);
                    RefreshList();
                    // Clear inputs
                    SetWindowTextW(g_hEditExt, L"");
                    SetWindowTextW(g_hEditName, L"");
                    SetWindowTextW(g_hEditPath, L"");
                } else {
                    MessageBoxW(hwnd, L"请填写完整信息 (后缀名, 名称, 路径)", L"提示", MB_OK | MB_ICONWARNING);
                }
            } else if (id == ID_BTN_REMOVE) {
                int idx = (int)SendMessage(g_hListAssoc, LB_GETCURSEL, 0, 0);
                if (idx != LB_ERR) {
                    removeFileAssociation(idx);
                    RefreshList();
                }
            } else if (id == ID_BTN_SAVE) {
                WCHAR buffer[MAX_PATH];
                GetWindowTextW(g_hEditEditor, buffer, MAX_PATH);
                lstrcpyW(g_editorPath, buffer);
                
                // Save font size
                WCHAR fontSizeStr[16];
                GetWindowTextW(g_hEditFontSize, fontSizeStr, 16);
                int fontSize = _wtoi(fontSizeStr);
                setFontSize(fontSize);
                
                saveSettings(); // This saves both editor path and associations
                DestroyWindow(hwnd);
            } else if (id == ID_BTN_CANCEL) {
                DestroyWindow(hwnd);
            } else if (id == ID_LIST_ASSOC && HIWORD(wParam) == LBN_SELCHANGE) {
                int idx = (int)SendMessage(g_hListAssoc, LB_GETCURSEL, 0, 0);
                if (idx != LB_ERR && idx < (int)g_associations.size()) {
                    const auto& assoc = g_associations[idx];
                    SetWindowTextW(g_hEditExt, assoc.ext.c_str());
                    SetWindowTextW(g_hEditName, assoc.name.c_str());
                    SetWindowTextW(g_hEditPath, assoc.command.c_str());
                }
            } else if (id == ID_COMBO_LANGUAGE && HIWORD(wParam) == CBN_SELCHANGE) {
                int sel = (int)SendMessage(g_hListLanguage, CB_GETCURSEL, 0, 0);
                if (sel != CB_ERR) {
                    setCurrentLanguage((LanguageType)sel);
                    // 提示用户重启程序以应用语言更改
                    MessageBoxW(hwnd, L"Language changed. Please restart the application to apply changes.", L"Info", MB_OK | MB_ICONINFORMATION);
                }
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            EnableWindow(GetParent(hwnd), TRUE);
            SetForegroundWindow(GetParent(hwnd));
            g_hDialog = NULL;
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ShowSettingsDialog(HWND parent) {
    if (g_hDialog) {
        SetForegroundWindow(g_hDialog);
        return;
    }
    
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = SettingsDlgProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"SettingsDialogClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    
    RECT rc;
    GetWindowRect(parent, &rc);
    int w = 550;
    int h = 650;  // 增加高度以容纳语言选择选项
    int x = rc.left + (rc.right - rc.left - w) / 2;
    int y = rc.top + (rc.bottom - rc.top - h) / 2;
    
    g_hDialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"SettingsDialogClass", getText(TEXT_SETTINGS_TITLE), 
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 
        x, y, w, h, parent, NULL, GetModuleHandle(NULL), NULL);
        
    if (g_hDialog) {
        EnableWindow(parent, FALSE);
    }
}
