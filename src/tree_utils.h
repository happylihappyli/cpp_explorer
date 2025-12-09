#ifndef TREE_UTILS_H
#define TREE_UTILS_H

#include <windows.h>
#include <commctrl.h>

// 获取逻辑驱动器
void getLogicalDrives(WCHAR drives[][4], int* count);

// 检查目录是否有子目录
BOOL hasSubdirectories(const WCHAR* path);

// 获取节点完整路径
void getNodeFullPath(HWND treeView, HTREEITEM hItem, WCHAR* fullPath, int bufferSize);

// 添加占位符节点
void addPlaceholderNode(HWND treeView, HTREEITEM parent);

// 展开目录节点
void expandDirectoryNode(HWND treeView, HTREEITEM hItem, const WCHAR* path);

// 更新目录树
void updateDirectoryTree();

// 处理树节点展开
void handleTreeItemExpanding(HWND treeView, HTREEITEM hItem);

// 保存树展开状态
void saveTreeExpansionState();

// 恢复树展开状态
void restoreTreeExpansionState();

#endif // TREE_UTILS_H