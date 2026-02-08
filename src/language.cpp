#include "language.h"
#include "file_utils.h"
#include <stdio.h>

// 简体中文文本资源
static const WCHAR* zhCNTexts[] = {
    // 主窗口
    L"文件资源管理器",
    
    // 工具栏按钮
    L"◀ 后退",
    L"▶ 前进",
    L"▲ 向上",
    L"+ 新建",
    L"▶ 前往",
    L"📂 打开",
    L"⚙ 设置",
    L"📁 折叠",
    
    // 树形视图
    L"★ 收藏夹",
    L"此电脑",
    L"磁盘详情",
    
    // 列表视图列
    L"名称",
    L"大小",
    L"类型",
    L"修改时间",
    L"创建时间",
    L"父目录",
    L"目录大小",
    
    // 标签页
    L"此电脑",
    L"磁盘详情",
    L"右键菜单管理",
    
    // 设置对话框
    L"设置",
    L"默认编辑器路径:",
    L"浏览...",
    L"字体大小 (6-24):",
    L"文件关联设置:",
    L"后缀名:",
    L"名称:",
    L"程序路径:",
    L"添加/更新",
    L"移除选中",
    L"保存全部",
    L"取消",
    L"界面语言:",
    L"简体中文",
    L"英语",
    
    // 消息提示
    L"无效的目录路径",
    L"请填写完整信息 (后缀名, 名称, 路径)",
    L"已设置为默认文件管理器。",
    L"设置失败，可能需要管理员权限或被安全软件拦截。",
    L"已恢复系统默认设置。",
    L"恢复失败，可能需要管理员权限。",
    
    // 右键菜单
    L"添加当前路径到收藏夹",
    L"移除此收藏项",
    L"编辑名称",
    L"删除到回收站",
    L"用 %s 打开",
    
    // 状态栏
    L"项",
    L"已选择",
    L"总大小"
};

// 英语文本资源
static const WCHAR* enUSTexts[] = {
    // Main Window
    L"File Explorer",
    
    // Toolbar Buttons
    L"◀ Back",
    L"▶ Forward",
    L"▲ Up",
    L"+ New Tab",
    L"▶ Go",
    L"📂 Open",
    L"⚙ Settings",
    L"📁 Collapse",
    
    // Tree View
    L"★ Favorites",
    L"This PC",
    L"Disk Details",
    
    // List View Columns
    L"Name",
    L"Size",
    L"Type",
    L"Modified",
    L"Created",
    L"Parent",
    L"Dir Size",
    
    // Tabs
    L"This PC",
    L"Disk Details",
    L"Context Menu Manager",
    
    // Settings Dialog
    L"Settings",
    L"Default Editor Path:",
    L"Browse...",
    L"Font Size (6-24):",
    L"File Associations:",
    L"Extension:",
    L"Name:",
    L"Program Path:",
    L"Add/Update",
    L"Remove Selected",
    L"Save All",
    L"Cancel",
    L"Interface Language:",
    L"Simplified Chinese",
    L"English",
    
    // Messages
    L"Invalid directory path",
    L"Please fill in complete information (extension, name, path)",
    L"Set as default file manager successfully.",
    L"Failed to set, may require administrator privileges or blocked by security software.",
    L"System default settings restored.",
    L"Failed to restore, may require administrator privileges.",
    
    // Context Menu
    L"Add current path to favorites",
    L"Remove this favorite",
    L"Edit name",
    L"Delete to Recycle Bin",
    L"Open with %s",
    
    // Status Bar
    L"items",
    L"selected",
    L"Total size"
};

// 当前语言
static LanguageType g_currentLanguage = LANG_ZH_CN;

// 获取当前语言
LanguageType getCurrentLanguage() {
    return g_currentLanguage;
}

// 设置当前语言
void setCurrentLanguage(LanguageType lang) {
    if (lang >= LANG_ZH_CN && lang <= LANG_EN_US) {
        g_currentLanguage = lang;
        saveLanguageSettings();
    }
}

// 获取指定ID的文本
const WCHAR* getText(TextID id) {
    if (id < 0 || id >= TEXT_MAX_ID) {
        return L"";
    }
    
    if (g_currentLanguage == LANG_EN_US) {
        return enUSTexts[id];
    } else {
        return zhCNTexts[id];
    }
}

// 初始化语言系统
void initLanguageSystem() {
    loadLanguageSettings();
}

// 保存语言设置
void saveLanguageSettings() {
    WCHAR path[MAX_PATH];
    getExecutableDirectory(path, MAX_PATH);
    lstrcatW(path, L"settings.ini");
    
    WCHAR langStr[16];
    wsprintfW(langStr, L"%d", g_currentLanguage);
    WritePrivateProfileStringW(L"Settings", L"Language", langStr, path);
}

// 加载语言设置
void loadLanguageSettings() {
    WCHAR path[MAX_PATH];
    getExecutableDirectory(path, MAX_PATH);
    lstrcatW(path, L"settings.ini");
    
    int lang = GetPrivateProfileIntW(L"Settings", L"Language", LANG_ZH_CN, path);
    if (lang >= LANG_ZH_CN && lang <= LANG_EN_US) {
        g_currentLanguage = (LanguageType)lang;
    } else {
        g_currentLanguage = LANG_ZH_CN;
    }
}
