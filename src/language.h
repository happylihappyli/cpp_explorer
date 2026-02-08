#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <windows.h>

// 支持的语言类型
enum LanguageType {
    LANG_ZH_CN = 0,  // 简体中文
    LANG_EN_US = 1   // 英语
};

// UI文本ID枚举
enum TextID {
    // 主窗口
    TEXT_WINDOW_TITLE,
    
    // 工具栏按钮
    TEXT_BTN_BACK,
    TEXT_BTN_FORWARD,
    TEXT_BTN_UP,
    TEXT_BTN_NEW_TAB,
    TEXT_BTN_GO,
    TEXT_BTN_OPEN,
    TEXT_BTN_SETTINGS,
    TEXT_BTN_COLLAPSE_DRIVES,
    
    // 树形视图
    TEXT_TREE_FAVORITES,
    TEXT_TREE_THIS_PC,
    TEXT_TREE_DISK_DETAILS,
    
    // 列表视图列
    TEXT_COL_NAME,
    TEXT_COL_SIZE,
    TEXT_COL_TYPE,
    TEXT_COL_MODIFIED,
    TEXT_COL_CREATED,
    TEXT_COL_PARENT,
    TEXT_COL_DIR_SIZE,
    
    // 标签页
    TEXT_TAB_THIS_PC,
    TEXT_TAB_DISK_DETAILS,
    TEXT_TAB_CONTEXT_MENU_MGR,
    
    // 设置对话框
    TEXT_SETTINGS_TITLE,
    TEXT_SETTINGS_EDITOR_PATH,
    TEXT_SETTINGS_BROWSE,
    TEXT_SETTINGS_FONT_SIZE,
    TEXT_SETTINGS_ASSOC,
    TEXT_SETTINGS_EXT,
    TEXT_SETTINGS_NAME,
    TEXT_SETTINGS_PROGRAM_PATH,
    TEXT_SETTINGS_ADD_UPDATE,
    TEXT_SETTINGS_REMOVE,
    TEXT_SETTINGS_SAVE_ALL,
    TEXT_SETTINGS_CANCEL,
    TEXT_SETTINGS_LANGUAGE,
    TEXT_SETTINGS_LANG_ZH_CN,
    TEXT_SETTINGS_LANG_EN_US,
    
    // 消息提示
    TEXT_MSG_INVALID_PATH,
    TEXT_MSG_COMPLETE_INFO,
    TEXT_MSG_SET_DEFAULT_SUCCESS,
    TEXT_MSG_SET_DEFAULT_FAILED,
    TEXT_MSG_RESTORE_DEFAULT_SUCCESS,
    TEXT_MSG_RESTORE_DEFAULT_FAILED,
    
    // 右键菜单
    TEXT_MENU_ADD_FAVORITE,
    TEXT_MENU_REMOVE_FAVORITE,
    TEXT_MENU_EDIT_NAME,
    TEXT_MENU_DELETE_TO_RECYCLE,
    TEXT_MENU_OPEN_WITH_EDITOR,
    
    // 状态栏
    TEXT_STATUS_ITEMS,
    TEXT_STATUS_SELECTED,
    TEXT_STATUS_TOTAL_SIZE,
    
    // 最大ID
    TEXT_MAX_ID
};

// 获取当前语言
LanguageType getCurrentLanguage();

// 设置当前语言
void setCurrentLanguage(LanguageType lang);

// 获取指定ID的文本
const WCHAR* getText(TextID id);

// 初始化语言系统
void initLanguageSystem();

// 保存语言设置
void saveLanguageSettings();

// 加载语言设置
void loadLanguageSettings();

#endif // LANGUAGE_H
