/*
    Author:     Adam
    Github:     https://gitee.com/AdamLoong
    Version:    4.0.0
    MIT License (详见顶部版权声明)
    简易可移植菜单库 - 实现文件
*/

#include "free_menu.h"

/************************** 全局变量 **************************/
Menu *hmenu = NULL;  // 当前聚焦的菜单句柄（全局焦点）

/************************** 内部宏定义 **************************/
// 计算菜单项文本的居中X坐标
#define MENU_CAL_ITEM_X(str) ((MENU_PANEL_WIDTH - (strlen(str) * MENU_FONT_WIDTH)) / 2)
// 计算菜单项文本的Y坐标（垂直居中）
#define MENU_CAL_ITEM_Y(index) ((index) * MENU_ITEM_HEIGHT + ((MENU_ITEM_HEIGHT - MENU_FONT_HEIGHT) / 2))

/************************** 内部静态函数 **************************/
/**
 * @brief 动画平滑过渡函数（用于光标/菜单项的渐变移动）
 * @param value 要过渡的变量指针
 * @param target 目标值
 */
static void smooth_transition(int *value, int target)
{
    if (*value != target)
    {
        // 基础步进（+1/-1）+ 比例步进（动画速度），实现平滑过渡
        (*value) += (target > *value) ? 1 : -1;
        (*value) += (target - *value) * MENU_ANIMATION_SPEED;
    }
}

/**
 * @brief 绘制菜单项（内部函数）
 * @param menu 菜单句柄
 */
static void menu_show_items(Menu *menu)
{
    if (menu == NULL)
        return;

    static int item_smooth = 0;    // 菜单项动画过渡值
    static int16_t ibase_prev = -1;// 上一次的菜单项基准索引
    int16_t ibase;                 // 当前屏显示的第一个菜单项索引（基准索引）
    int16_t inum;                  // 循环变量（当前绘制的菜单项索引）
    int16_t icnt;                  // 当前屏要绘制的菜单项总数

    inum = MENU_TITLE;  // 起始索引（跳过标题行）
    // 计算基准索引：当前选中项 - 光标相对索引
    ibase = menu->item_index - menu->cursor_index;
    if (ibase < 0)      // 基准索引不能为负
    {
        ibase = 0;
        menu->cursor_index = menu->item_index; // 修正光标相对索引
    }
    // 计算当前屏可显示的菜单项数（避免超出总数）
    icnt = (ibase + menu->cursor_limit > menu->item_count) ? (menu->item_count - ibase) : menu->cursor_limit;

    // 基准索引变化时，初始化动画过渡值
    if (ibase != ibase_prev)
    {
        item_smooth = (ibase - ibase_prev) > 0 ? MENU_ITEM_HEIGHT : -MENU_ITEM_HEIGHT;
        ibase_prev = ibase;
    }

    // 执行动画过渡
    smooth_transition(&item_smooth, 0);

    // 动画期间调整绘制范围（避免画面断层）
    if (item_smooth != 0)
    {
        if (item_smooth < 0)
            inum++;  // 向上滚动时，跳过第一个项
        else
            icnt--;  // 向下滚动时，减少最后一个项
    }

    // 绘制当前屏的所有菜单项
    while (inum < icnt)
    {
        // 获取菜单项名称
        char *name = (char *)menu->item_cb(menu->items, ibase + inum, MENU_ITEM_CMD_NAME);

        // 更新光标宽度（适配当前选中项的文本长度）
        if (ibase + inum == menu->item_index)
        {
            menu->cursor_width = strlen(name) * MENU_FONT_WIDTH + MENU_ITEM_XOFFSET * 2;
        }

        // 绘制菜单项文本（带动画偏移）
        menu_hal_draw_string(MENU_ITEM_XOFFSET, MENU_CAL_ITEM_Y(inum) + item_smooth, name);

        inum++;
    }

    // 绘制菜单标题（如果启用）
    // if (MENU_TITLE)
    // {
    //     char *name = (char *)menu->item_cb(menu->items, 0, MENU_ITEM_CMD_NAME);
    //     // 标题居中绘制
    //     menu_hal_draw_string(MENU_CAL_ITEM_X(name), MENU_CAL_ITEM_Y(0), name);
    //     // 标题行显示返回标识（<<）
    //     if (menu->cursor_index == 0)
    //         menu_hal_draw_string(0, MENU_CAL_ITEM_Y(0), "<<");
    // }
    // 绘制菜单标题（如果启用）
    if (MENU_TITLE)
    {
        // 防护1：检查回调函数是否为空
        if (menu->item_cb == NULL)
        {
            // 回调为空时，显示默认标题，避免调用空函数
            menu_hal_draw_string(MENU_CAL_ITEM_X("Menu Title"), MENU_CAL_ITEM_Y(0), "Menu Title");
            return;
        }
        
        // 防护2：调用回调前，检查items是否为空（避免回调内部访问NULL）
        void *items = menu->items;
        if (items == NULL)
        {
            menu_hal_draw_string(MENU_CAL_ITEM_X("No Items"), MENU_CAL_ITEM_Y(0), "No Items");
            return;
        }
        
        // 防护3：调用回调并检查返回值
        char *name = (char *)menu->item_cb(items, 0, MENU_ITEM_CMD_NAME);
        if (name == NULL || strlen(name) == 0)
        {
            // 返回值为空时，显示默认字符串
            name = "Main Menu";
        }

        // 标题居中绘制
        menu_hal_draw_string(MENU_CAL_ITEM_X(name), MENU_CAL_ITEM_Y(0), name);
        // 标题行显示返回标识（<<）- 增加光标索引合法性检查
        if (menu->cursor_index == 0 && menu->item_index == 0 && name != NULL)
            menu_hal_draw_string(0, MENU_CAL_ITEM_Y(0), "<<");
    }
}

/**
 * @brief 绘制光标（带动画，内部函数）
 * @param x 光标目标X坐标
 * @param y 光标目标Y坐标
 * @param w 光标目标宽度
 * @param h 光标目标高度
 * @param cursor_style 光标类型（MENU_CURSOR_TYPE_*）
 */
static void menu_show_cursor(int x, int y, int w, int h, int cursor_style)
{
    // 光标动画过渡变量（静态保存上次值）
    static int x_smooth = 0, y_smooth = 0, w_smooth = 0, h_smooth = 0;
    
    // 执行各维度的平滑过渡
    smooth_transition(&x_smooth, x);
    smooth_transition(&y_smooth, y);
    smooth_transition(&w_smooth, w);
    h_smooth = h;  // 高度无动画（直接赋值）

    // 调用硬件层绘制光标
    menu_hal_draw_cursor(x_smooth, y_smooth, w_smooth, h_smooth, cursor_style);
}

/************************** 核心API实现 **************************/
/**
 * @brief 初始化菜单
 * @param menu 菜单句柄
 * @param items 菜单项数据集合（数组/链表）
 * @param item_cb 菜单项处理回调函数
 */
void menu_init(Menu *menu, void *items, MenuItemHandler item_cb)
{
    if (menu == NULL)
        return;

    // 初始化菜单基础参数
    menu->items = items;
    menu->item_cb = item_cb;
    // 获取菜单项总数
    menu->item_count = (int)menu->item_cb(menu->items, 1, MENU_ITEM_CMD_COUNT);

    // 初始化选中状态
    menu->item_index = 0;
    menu->cursor_index = 0;  // 光标相对索引不能超过选中项索引

    // 计算单屏最大可显示菜单项数
    menu->cursor_limit = MENU_PANEL_HEIGHT / MENU_ITEM_HEIGHT;

    // 单屏显示数不能超过总项数
    if (menu->cursor_limit > menu->item_count)
    {
        menu->cursor_limit = menu->item_count;
    }

    // 初始化滚轮事件（修正初始状态）
    menu_event_wheel(menu, 1);
}

/**
 * @brief 注入菜单事件
 * @param menu 菜单句柄
 * @param event 事件类型（MENU_EVENT_*）
 */
void menu_event_inject(Menu *menu, uint32_t event)
{
    if (menu == NULL)
        return;

    switch (event)
    {
    case MENU_EVENT_ENTER:  // 确认/进入事件
        // 标题行/越界索引：执行返回逻辑
        if (menu->item_index < MENU_TITLE || menu->item_index >= menu->item_count)
        {
            menu->item_cb(menu->items, 0, MENU_ITEM_CMD_BACK);
        }
        else  // 普通菜单项：执行选中项逻辑
        {
            menu->item_cb(menu->items, menu->item_index, MENU_ITEM_CMD_ENTER);
        }
        break;

    case MENU_EVENT_BACK:  // 返回/退出事件
        menu->item_cb(menu->items, 0, MENU_ITEM_CMD_BACK);
        break;

    case MENU_EVENT_WHEEL_UP:  // 滚轮向上
        menu_event_wheel(menu, -1);
        break;

    case MENU_EVENT_WHEEL_DOWN:  // 滚轮向下
        menu_event_wheel(menu, +1);
        break;

    default:
        break;
    }
}

/**
 * @brief 处理滚轮事件（上下选）
 * @param menu 菜单句柄
 * @param wheel 滚轮方向（+1=下，-1=上）
 */
void menu_event_wheel(Menu *menu, int wheel)
{
    if (menu == NULL)
        return;

    // 更新选中项索引和光标相对索引
    menu->item_index += wheel;
    menu->cursor_index += wheel;

    // 边界检查：选中项索引不能超出范围
    if (menu->item_index >= menu->item_count)
        menu->item_index = menu->item_count - 1;
    else if (menu->item_index < MENU_TITLE)
        menu->item_index = menu->cursor_index < MENU_TITLE ? 0 : MENU_TITLE;

    // 边界检查：光标相对索引不能超出单屏范围
    if (menu->cursor_index >= menu->cursor_limit)
        menu->cursor_index = menu->cursor_limit - 1;
    else if (menu->cursor_index < MENU_TITLE)
        menu->cursor_index = MENU_TITLE;

    // 光标相对索引不能超过当前选中项索引
    if (menu->cursor_index > menu->item_index)
        menu->cursor_index = menu->item_index;
}

/**
 * @brief 显示菜单（绘制所有元素：菜单项、光标、滚动条）
 * @param menu 菜单句柄
 * @return 当前聚焦的菜单句柄
 */
Menu *menu_show(Menu *menu)
{
    if (menu == NULL)
        return hmenu;

    // 绘制菜单项
    menu_show_items(menu);
    // 绘制普通光标
    menu_show_cursor(0,                                                                             // 光标X坐标
                     menu->cursor_index * MENU_ITEM_HEIGHT,                                         // 光标Y坐标
                     (MENU_TITLE && menu->item_index == 0) ? MENU_PANEL_WIDTH : menu->cursor_width, // 光标宽度（标题行占满全屏）
                     MENU_ITEM_HEIGHT,                                                              // 光标高度
                     MENU_CURSOR_TYPE_NORMAL);                                                      // 普通光标类型

    // 绘制滚动条（标题行索引修正为1，避免分母为0）
    menu_hal_draw_scroll(menu->item_index > 1 ? menu->item_index : 1, menu->item_count - 1);

    return hmenu;
}

/**
 * @brief 显示菜单并绘制编辑模式光标
 * @param menu 菜单句柄
 * @param edit_offset 编辑光标相对于文本的偏移（字符数）
 * @param edit_w 编辑光标宽度（字符数）
 * @return 当前聚焦的菜单句柄
 */
Menu *menu_show_edit_cursor(Menu *menu, int edit_offset, int edit_w)
{
    if (menu == NULL)
        return hmenu;

    // 绘制菜单项
    menu_show_items(menu);

    // 绘制编辑模式光标
    menu_show_cursor(MENU_ITEM_XOFFSET + edit_offset * MENU_FONT_WIDTH,                                   // 编辑光标X坐标（字符偏移）
                     menu->cursor_index * MENU_ITEM_HEIGHT,                                               // 编辑光标Y坐标
                     (MENU_TITLE && menu->item_index == 0) ? MENU_PANEL_WIDTH : edit_w * MENU_FONT_WIDTH, // 编辑光标宽度
                     MENU_ITEM_HEIGHT,                                                                    // 编辑光标高度
                     MENU_CURSOR_TYPE_EDIT);                                                              // 编辑光标类型

    // 绘制滚动条
    menu_hal_draw_scroll(menu->item_index > 1 ? menu->item_index : 1, menu->item_count - 1);

    return hmenu;
}

/**
 * @brief 直接选中指定索引的菜单项
 * @param menu 菜单句柄
 * @param item_index 目标菜单项索引
 */
void menu_select_item(Menu *menu, int item_index)
{
    if (menu == NULL)
        return;

    // 重置选中索引，通过滚轮事件修正到目标值
    menu->item_index = 0;
    menu_event_wheel(menu, item_index);
}

/**
 * @brief 设置菜单为当前焦点
 * @param menu 要聚焦的菜单句柄
 */
void menu_set_focus(Menu *menu)
{
    if (menu == NULL)
    {
        hmenu = NULL;
        return;
    }

    // 检查菜单是否已在链表中（避免重复添加）
    Menu *p = hmenu;
    while (p)
    {
        if (p == menu)
        {
            break;
        }
        p = p->next;
    }
    // 不在链表中则添加到表头
    if (p == NULL)
    {
        menu->next = hmenu;
    }
    // 设置为当前焦点
    hmenu = menu;
}

/**
 * @brief 返回上一级菜单（焦点回退）
 */
void menu_back_focus(void)
{
    if (hmenu)
        hmenu = hmenu->next;  // 焦点切换到链表下一个节点（上一级）
}

/************************** 菜单项处理函数实现 **************************/
/**
 * @brief 函数型菜单项的处理回调
 * @param items 菜单项数组（MenuItemTypeFunc类型）
 * @param index 菜单项索引
 * @param cmd 命令（MENU_ITEM_CMD_*）
 * @return 命令执行结果
 */
void *menu_item_handler_func(void *items_, int index, int cmd)
{
    MenuItemTypeFunc *items = (MenuItemTypeFunc *)items_;
    if (items == NULL || index < 0)
        return "null";

    void *ret = NULL;
    switch (cmd)
    {
    case MENU_ITEM_CMD_NAME:  // 获取名称
        ret = items[index](false);
        break;
    case MENU_ITEM_CMD_ENTER: // 执行功能
        if (index == 0)
            menu_back_focus();  // 标题行执行返回
        else
            items[index](true); // 普通项执行函数
        break;
    case MENU_ITEM_CMD_BACK:  // 返回逻辑
        menu_back_focus();
        break;
    case MENU_ITEM_CMD_COUNT: // 获取总数
    {
        int count = 0;
        while (items[count])
            count++;
        ret = (void *)count;
        break;
    }
    }
    return ret;
}

/**
 * @brief 结构体类型菜单项的处理回调
 * @param items 菜单项数组（MenuItemTypeStru类型）
 * @param index 菜单项索引
 * @param cmd 命令（MENU_ITEM_CMD_*）
 * @return 命令执行结果
 */
void *menu_item_handler_stru(void *items_, int index, int cmd)
{
    MenuItemTypeStru *items = (MenuItemTypeStru *)items_;
    if (items == NULL || index < 0)
        return "null";

    void *ret = NULL;
    switch (cmd)
    {
    case MENU_ITEM_CMD_NAME:  // 获取名称
        ret = items[index].name;
        break;
    case MENU_ITEM_CMD_ENTER: // 执行功能
        if (items[index].func)
            items[index].func();
        break;
    case MENU_ITEM_CMD_BACK:  // 返回逻辑
        menu_back_focus();
        break;
    case MENU_ITEM_CMD_COUNT: // 获取总数
    {
        int count = 0;
        while (items[count].name)
            count++;
        ret = (void *)count;
        break;
    }
    }
    return ret;
}

/**
 * @brief 链表类型菜单项的处理回调
 * @param items 菜单项链表头（MenuItemTypeList类型）
 * @param index 菜单项索引
 * @param cmd 命令（MENU_ITEM_CMD_*）
 * @return 命令执行结果
 */
void *menu_item_handler_list(void *items_, int index, int cmd)
{
    volatile MenuItemTypeList *items = (MenuItemTypeList *)items_;
    if (items == NULL || index < 0)
        return "null";

    void *ret = NULL;
    switch (cmd)
    {
    case MENU_ITEM_CMD_NAME:  // 获取名称
        // 遍历链表到指定索引
        while (items && index--)
            items = items->next;
        ret = items->name;
        break;
    case MENU_ITEM_CMD_ENTER: // 执行功能
        // 遍历链表到指定索引
        while (items && index--)
            items = items->next;
        if (items->func)
            items->func();
        break;
    case MENU_ITEM_CMD_BACK:  // 返回逻辑
        menu_back_focus();
        break;
    case MENU_ITEM_CMD_COUNT: // 获取总数
    {
        int count = 0;
        while (items)
        {
            items = items->next;
            count++;
        }
        ret = (void *)count;
        break;
    }
    }
    return ret;
}

/**
 * @brief 字符串数组型菜单项的处理回调
 * @param items 字符串数组（char**类型）
 * @param index 菜单项索引
 * @param cmd 命令（MENU_ITEM_CMD_*）
 * @return 命令执行结果
 */
void *menu_item_handler_char(void *items_, int index, int cmd)
{
    char **items = (char **)items_;

    if (items == NULL || index < 0)
        return "null";

    void *ret = NULL;
    switch (cmd)
    {
    case MENU_ITEM_CMD_NAME:  // 获取名称
        ret = items[index];
        break;
    case MENU_ITEM_CMD_ENTER: // 执行功能（字符串数组无执行逻辑）
        break;
    case MENU_ITEM_CMD_BACK:  // 返回逻辑
        menu_back_focus();
        break;
    case MENU_ITEM_CMD_COUNT: // 获取总数
    {
        int count = 0;
        while (items[count])
            count++;
        ret = (void *)count;
        break;
    }
    }
    return ret;
}

/**
 * @brief 向链表型菜单添加菜单项
 * @param menu 菜单句柄
 * @param name 菜单项名称
 * @param func 菜单项执行函数（NULL=无操作）
 */
void menu_item_list_add(Menu *menu, char *name, void (*func)(void))
{
    if (menu == NULL)
        return;

    // 分配新节点内存
    MenuItemTypeList *item = (MenuItemTypeList *)malloc(sizeof(MenuItemTypeList));
    item->name = name;
    item->func = func;
    item->next = NULL;

    // 找到链表尾节点
    MenuItemTypeList *p = menu->items;
    if (p == NULL)
    {
        // 链表为空，新节点作为头
        menu->items = item;
    }
    else
    {
        // 遍历到尾节点
        while (p->next)
            p = p->next;
        p->next = item;
    }

    // 重新初始化菜单（更新项数）
    menu_init(menu, menu->items, menu_item_handler_list);
}
