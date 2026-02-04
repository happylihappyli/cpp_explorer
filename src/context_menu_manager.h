#ifndef CONTEXT_MENU_MANAGER_H
#define CONTEXT_MENU_MANAGER_H

#include <windows.h>

// 初始化右键菜单管理页面（创建控件，但默认隐藏）
void InitContextMenuManager(HWND parent);

// 显示或隐藏右键菜单管理页面
void ShowContextMenuManager(BOOL show);

// 调整右键菜单管理页面大小
void ResizeContextMenuManager(RECT rect);

// 处理右键菜单管理页面的消息
BOOL HandleContextMenuManagerMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif // CONTEXT_MENU_MANAGER_H
