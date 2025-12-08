/*
    Author:     Adam
    Github:     https://gitee.com/AdamLoong
    Version:    4.0.0
    MIT License (详见顶部版权声明)
    简易可移植菜单库 - 头文件
    适用于嵌入式系统/小型GUI的菜单管理，支持多级菜单、动画光标、滚动条等特性
*/

#ifndef _FREE_MENU_H_
#define _FREE_MENU_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/************************** 移植层接口（需用户实现） **************************/
/**
 * @brief 绘制字符串的硬件抽象层函数（需移植实现）
 * @param x 绘制起始X坐标
 * @param y 绘制起始Y坐标
 * @param str 要绘制的字符串
 */
void menu_hal_draw_string(int x, int y, const char *str);

/**
 * @brief 绘制光标的硬件抽象层函数（需移植实现）
 * @param x 光标X坐标
 * @param y 光标Y坐标
 * @param w 光标宽度
 * @param h 光标高度
 * @param cursor_type 光标类型（MENU_CURSOR_TYPE_*）
 */
void menu_hal_draw_cursor(int x, int y, int w, int h, int cursor_type);

/**
 * @brief 绘制滚动条的硬件抽象层函数（需移植实现）
 * @param numerator 当前滚动位置（分子）
 * @param denominator 总滚动范围（分母）
 */
void menu_hal_draw_scroll(int numerator, int denominator);

/************************** 菜单配置参数（可自定义） **************************/
#define MENU_PANEL_WIDTH     (128)      // 菜单面板总宽度（像素）
#define MENU_PANEL_HEIGHT    (64)       // 菜单面板总高度（像素）
#define MENU_ITEM_XOFFSET    (4)        // 菜单项文本X轴偏移（避免贴边）
#define MENU_ITEM_HEIGHT     (10)       // 单个菜单项的高度（像素）
#define MENU_FONT_WIDTH      (6)        // 字体宽度（像素/字符）
#define MENU_FONT_HEIGHT     (8)       // 字体高度（像素）
#define MENU_ANIMATION_SPEED (0.12f)    // 光标/菜单项动画速度[0,1]，1=无动画
#define MENU_TITLE           (1)        // 是否显示菜单标题（1=显示，0=不显示）

/************************** 菜单事件定义 **************************/
#define MENU_EVENT_ENTER     1   // 确认/进入事件
#define MENU_EVENT_BACK      2   // 返回/退出事件
#define MENU_EVENT_WHEEL_UP  3   // 滚轮向上/上选事件
#define MENU_EVENT_WHEEL_DOWN 4  // 滚轮向下/下选事件

/************************** 菜单项命令定义 **************************/
#define MENU_ITEM_CMD_NAME   0   // 获取菜单项名称
#define MENU_ITEM_CMD_ENTER  1   // 执行菜单项逻辑
#define MENU_ITEM_CMD_BACK   2   // 执行返回逻辑
#define MENU_ITEM_CMD_COUNT  3   // 获取菜单项总数

/************************** 光标类型定义 **************************/
#define MENU_CURSOR_TYPE_NORMAL 0  // 普通选择光标
#define MENU_CURSOR_TYPE_EDIT   1  // 编辑模式光标（如输入框）
#define MENU_CURSOR_TYPE_NONE   2  // 无光标

/************************** 类型定义 **************************/
/**
 * @brief 菜单项处理函数回调类型
 * @param items 菜单项数据集合（不同类型的菜单项数组/链表）
 * @param index 菜单项索引
 * @param cmd 要执行的命令（MENU_ITEM_CMD_*）
 * @return 命令执行结果（如名称字符串、数量等）
 */
typedef void *(*MenuItemHandler)(void *items, int index, int cmd);

/**
 * @brief 函数型菜单项的回调函数类型
 * @param action false=获取名称，true=执行功能
 * @return 菜单项名称（action=false时）
 */
typedef char *(*MenuItemTypeFunc)(bool action);

/**
 * @brief 结构体类型菜单项（固定数组）
 */
typedef struct _MenuItemTypeStru
{
    char *name;           // 菜单项显示名称
    void (*func)(void);   // 菜单项执行函数（NULL=无操作）
} MenuItemTypeStru;

/**
 * @brief 链表类型菜单项（动态添加）
 */
typedef struct _MenuItemTypeList
{
    struct _MenuItemTypeList *next;  // 下一个菜单项节点
    char *name;                      // 菜单项显示名称
    void (*func)(void);              // 菜单项执行函数（NULL=无操作）
} MenuItemTypeList;

/**
 * @brief 菜单核心结构体
 */
typedef struct _Menu
{
    struct _Menu *next;        // 下一级/上一级菜单链表指针（用于多级菜单）
    void *items;               // 菜单项数据集合（数组/链表）
    MenuItemHandler item_cb;   // 菜单项处理回调函数

    int16_t item_count;        // 菜单项总数
    int16_t item_index;        // 当前选中的菜单项索引

    int16_t cursor_width;      // 光标宽度（适配菜单项文本长度）
    int8_t cursor_limit;       // 单屏可显示的最大菜单项数
    int8_t cursor_index;       // 光标在屏幕内的相对索引（0~cursor_limit-1）
} Menu;

/************************** 全局变量声明 **************************/
extern Menu *hmenu;  // 当前聚焦的菜单句柄（全局焦点）

/************************** 核心API函数声明 **************************/
/**
 * @brief 初始化菜单
 * @param menu 菜单句柄
 * @param items 菜单项数据集合（数组/链表）
 * @param item_cb 菜单项处理回调函数
 */
void menu_init(Menu *menu, void *items, MenuItemHandler item_cb);

/**
 * @brief 注入菜单事件
 * @param menu 菜单句柄
 * @param event 事件类型（MENU_EVENT_*）
 */
void menu_event_inject(Menu *menu, uint32_t event);

/**
 * @brief 处理滚轮事件（上下选）
 * @param menu 菜单句柄
 * @param wheel 滚轮方向（+1=下，-1=上）
 */
void menu_event_wheel(Menu *menu, int wheel);

/**
 * @brief 显示菜单（绘制所有元素）
 * @param menu 菜单句柄
 * @return 当前聚焦的菜单句柄
 */
Menu *menu_show(Menu *menu);

/**
 * @brief 显示菜单并绘制编辑模式光标
 * @param menu 菜单句柄
 * @param edit_offset 编辑光标相对于文本的偏移（字符数）
 * @param edit_w 编辑光标宽度（字符数）
 * @return 当前聚焦的菜单句柄
 */
Menu *menu_show_edit_cursor(Menu *menu, int edit_offset, int edit_w);

/**
 * @brief 直接选中指定索引的菜单项
 * @param menu 菜单句柄
 * @param item_index 目标菜单项索引
 */
void menu_select_item(Menu *menu, int item_index);

/**
 * @brief 设置菜单为当前焦点
 * @param menu 要聚焦的菜单句柄
 */
void menu_set_focus(Menu *menu);

/**
 * @brief 返回上一级菜单（焦点回退）
 */
void menu_back_focus(void);

/************************** 快捷宏定义 **************************/
#define menu_get_focus() (hmenu)  // 获取当前聚焦的菜单句柄
// 显示当前聚焦的菜单
#define menu_show_focus() (menu_show(menu_get_focus()))
// 显示当前聚焦的菜单并绘制编辑光标
#define menu_show_focus_edit_cursor(edit_offset, edit_w) (menu_show_edit_cursor(menu_get_focus(), edit_offset, edit_w))
// 向当前聚焦的菜单注入事件
#define menu_event_inject_focus(event) (menu_event_inject(menu_get_focus(), event))
// 向当前聚焦的菜单注入滚轮事件
#define menu_event_wheel_focus(wheel) (menu_event_wheel(menu_get_focus(), wheel))

/************************** 菜单项处理函数（预制） **************************/
/**
 * @brief 函数型菜单项的处理回调
 * @param items 菜单项数组（MenuItemTypeFunc类型）
 * @param index 菜单项索引
 * @param cmd 命令（MENU_ITEM_CMD_*）
 * @return 命令执行结果
 */
void *menu_item_handler_func(void *items, int index, int cmd);

/**
 * @brief 结构体类型菜单项的处理回调
 * @param items 菜单项数组（MenuItemTypeStru类型）
 * @param index 菜单项索引
 * @param cmd 命令（MENU_ITEM_CMD_*）
 * @return 命令执行结果
 */
void *menu_item_handler_stru(void *items, int index, int cmd);

/**
 * @brief 字符串数组型菜单项的处理回调
 * @param items 字符串数组（char**类型）
 * @param index 菜单项索引
 * @param cmd 命令（MENU_ITEM_CMD_*）
 * @return 命令执行结果
 */
void *menu_item_handler_char(void *items, int index, int cmd);

/**
 * @brief 链表类型菜单项的处理回调
 * @param items 菜单项链表头（MenuItemTypeList类型）
 * @param index 菜单项索引
 * @param cmd 命令（MENU_ITEM_CMD_*）
 * @return 命令执行结果
 */
void *menu_item_handler_list(void *items, int index, int cmd);

/**
 * @brief 向链表型菜单添加菜单项
 * @param menu 菜单句柄
 * @param name 菜单项名称
 * @param func 菜单项执行函数（NULL=无操作）
 */
void menu_item_list_add(Menu *menu, char *name, void (*func)(void));

#endif // _FREE_MENU_H_
