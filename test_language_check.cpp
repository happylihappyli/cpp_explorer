#include <windows.h>
#include <stdio.h>

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

// 简体中文文本资源
static const WCHAR* zhCNTexts[] = {
    // 主窗口
    L"文件资源管理器",
    
    // 工具栏按钮
    L"←",
    L"→",
    L"↑",
    L"+",
    L"前往",
    L"打开",
    L"设置",
    L"折叠驱动器",
    
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

int main() {
    printf("Text Array Index Check:\n");
    printf("========================\n");
    printf("TEXT_SETTINGS_LANGUAGE (index %d): %ls\n", TEXT_SETTINGS_LANGUAGE, zhCNTexts[TEXT_SETTINGS_LANGUAGE]);
    printf("TEXT_SETTINGS_LANG_ZH_CN (index %d): %ls\n", TEXT_SETTINGS_LANG_ZH_CN, zhCNTexts[TEXT_SETTINGS_LANG_ZH_CN]);
    printf("TEXT_SETTINGS_LANG_EN_US (index %d): %ls\n", TEXT_SETTINGS_LANG_EN_US, zhCNTexts[TEXT_SETTINGS_LANG_EN_US]);
    printf("========================\n");
    printf("Total items in array: %d\n", sizeof(zhCNTexts) / sizeof(zhCNTexts[0]));
    printf("TEXT_MAX_ID: %d\n", TEXT_MAX_ID);
    
    if (sizeof(zhCNTexts) / sizeof(zhCNTexts[0]) == TEXT_MAX_ID) {
        printf("\n✓ Array size matches TEXT_MAX_ID\n");
    } else {
        printf("\n✗ Array size does NOT match TEXT_MAX_ID\n");
        printf("  Array has %d items, but TEXT_MAX_ID is %d\n", 
               (int)(sizeof(zhCNTexts) / sizeof(zhCNTexts[0])), TEXT_MAX_ID);
    }
    
    return 0;
}
