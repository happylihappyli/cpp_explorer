#ifndef SETTINGS_H
#define SETTINGS_H

#include <windows.h>
#include <vector>
#include <string>

struct FileAssociation {
    std::wstring ext;
    std::wstring name;
    std::wstring command;
};

void loadSettings();
void getEditorPath(WCHAR* buffer, int bufferSize);
void setEditorPath(const WCHAR* path);
void ShowSettingsDialog(HWND parent);

// 字体大小设置
int getFontSize();
void setFontSize(int size);

// File Association functions
std::vector<FileAssociation> getFileAssociations();
void addFileAssociation(const std::wstring& ext, const std::wstring& name, const std::wstring& command);
void removeFileAssociation(int index);

#endif
