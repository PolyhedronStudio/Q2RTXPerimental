/*
** Copyright (c) 2024 rxi
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the MIT license. See `microui.c` for details.
*/
/********************************************************************
*
*
*	MicroUI: Immediate Mode Graphical User Interface.
*		- https://github.com/rxi/microui
*
*	"Forked", and customized to our needs, by WID in 2026 for 
*	use in the Q2RTXP client game module, as the core of our 
*	in-game UI system, which we call "GameUI".
*
********************************************************************/

#ifndef MICROUI_H
#define MICROUI_H

//#define MU_VERSION "2.02"
#define MU_VERSION "GameUI 1.0"

//! The maximum number of characters in a text command, including the null terminator.
#define MU_COMMANDLIST_SIZE     (256 * 1024)
//! The maximum number of containers that can be active at the same time. This includes all parent and child containers.
#define MU_ROOTLIST_SIZE        32
//! The maximum number of items that can be in a container's layout stack. This is used for nested layouts within a container.
#define MU_CONTAINERSTACK_SIZE  32
//! The maximum number of items that can be clipped on a container's layout stack. This is used for nested layouts within a container.#define MU_CONTAINERSTACK_SIZE  32
#define MU_CLIPSTACK_SIZE       32
//! The maximum number of IDs that can be in the ID stack. This is used for generating unique IDs for UI elements.
#define MU_IDSTACK_SIZE         32
//! The maximum number of layouts that can be in the layout stack. This is used for nested layouts within a container.
#define MU_LAYOUTSTACK_SIZE     16
//! The maximum number of items that can be in a container's layout stack. This is used for nested layouts within a container.
#define MU_CONTAINERPOOL_SIZE   48
//! The maximum number of items that can be in a container's layout stack. This is used for nested layouts within a container.
#define MU_TREENODEPOOL_SIZE    48
//! The maximum width of a container's layout stack. This is used for nested layouts within a container.
#define MU_MAX_WIDTHS           16
//! The type of real numbers used for sliders and other UI elements that require floating point values. This can be changed to double if higher precision is needed, but it will increase the size of the UI context and commands.
#define MU_REAL                 float
//! The format string used for printing real numbers in text commands. This should be consistent with the type defined in MU_REAL, and it should be chosen to balance precision and readability.
#define MU_REAL_FMT             "%.3g"
//! The format string used for printing slider values in text commands. This should be consistent with the type defined in MU_REAL, and it should be chosen to balance precision and readability. It is recommended to use a fixed number of decimal places for sliders, to avoid showing too many digits when the value is close to zero.
#define MU_SLIDER_FMT           "%.2f"
//! Maximum length of a formatted string in a text command, including the null terminator. This should be chosen to balance the need for long strings and the size of the UI context and commands. It is recommended to keep this value relatively small, as it will affect the performance of the UI system when processing text commands.
#define MU_MAX_FMT              127



/**
*	Macro to define a stack structure for a given type and size. This is used for the ID stack, layout stack, 
*	and container stack in the UI context. 
*
*	The stack structure contains an index to keep track of the current position in the stack, and an array of 
*	items of the specified type and size. The macro allows us to easily define multiple stacks with different 
*	types and sizes without having to write separate struct definitions for each one.
**/
//! For example, mu_stack(int, 16) will define a stack structure that can hold up to 16 integers, and it will have an index to keep track of how many integers are currently in the stack.
#define mu_stack(T, n)          struct { int idx; T items[n]; }
//! Minimum macro, returns the smaller of a and b. This is used for clamping values and other operations where we need to ensure a value does not exceed a certain limit.
#define mu_min(a, b)            ((a) < (b) ? (a) : (b))
//! Maximum macro, returns the larger of a and b. This is used for clamping values and other operations where we need to ensure a value does not go below a certain limit.
#define mu_max(a, b)            ((a) > (b) ? (a) : (b))
//! Clamp macro, returns x clamped between a and b. This is used for ensuring values stay within a certain range, such as when scrolling or resizing UI elements.
#define mu_clamp(x, a, b)       mu_min(b, mu_max(a, x))

/**
*	@brief	The clip mode for a clipping command. This is used to specify whether the clipping region should be applied to the current container only, or to all child containers as well. The MU_CLIP_PART mode will apply the clipping region to the current container only, while the MU_CLIP_ALL mode will apply it to all child containers as well. 
*			This allows for more flexible and efficient clipping of UI elements, as we can choose to clip only the necessary parts of the UI instead of applying a global clipping region that may affect performance.
**/
enum {
	//! Clip mode for a clipping command, specifies whether the clipping region should be applied to the current container only, or to all child containers as well.
	MU_CLIP_PART = 1,
	//! Clip mode for a clipping command, specifies whether the clipping region should be applied to the current container only, or to all child containers as well.
	MU_CLIP_ALL
};

/**
*	@details	The UI builds a packed command list (mu_Command entries). 
*				Each command starts with a mu_BaseCommand (type + size). 
*				These enum values identify the concrete command payload that 
*				follows and are used by the renderer/command-stepper to 
*				decode and execute UI drawing and control flow.
**/
enum {
	//! Jump/branch command used to alter the current command pointer. Emitted for root/container head/tail linking so the command iterator can skip or chain containers.
	MU_COMMAND_JUMP = 1,
	//! Clipping command that sets the current clip rectangle for subsequent draw commands.
	MU_COMMAND_CLIP,
	//! Draw-rect command for filled rectangles. Contains rect + color.
	MU_COMMAND_RECT,
	//! Draw-text command. Contains font, position, color and a null-terminated string immediately after the command header.
	MU_COMMAND_TEXT,
	//! Draw-icon command. Contains an icon id, destination rect and color.
	MU_COMMAND_ICON,
	//! Sentinel value (one past the last valid command type). Useful for bounds checks and arrays sized by command type.
	MU_COMMAND_MAX
};

enum {
  MU_COLOR_TEXT,
  MU_COLOR_BORDER,
  MU_COLOR_WINDOWBG,
  MU_COLOR_TITLEBG,
  MU_COLOR_TITLETEXT,
  MU_COLOR_PANELBG,
  MU_COLOR_BUTTON,
  MU_COLOR_BUTTONHOVER,
  MU_COLOR_BUTTONFOCUS,
  MU_COLOR_BASE,
  MU_COLOR_BASEHOVER,
  MU_COLOR_BASEFOCUS,
  MU_COLOR_SCROLLBASE,
  MU_COLOR_SCROLLTHUMB,
  MU_COLOR_MAX
};

enum {
  MU_ICON_CLOSE = 1,
  MU_ICON_CHECK,
  MU_ICON_COLLAPSED,
  MU_ICON_EXPANDED,
  MU_ICON_MAX
};

enum {
  MU_RES_ACTIVE       = (1 << 0),
  MU_RES_SUBMIT       = (1 << 1),
  MU_RES_CHANGE       = (1 << 2)
};

enum {
  MU_OPT_ALIGNCENTER  = (1 << 0),
  MU_OPT_ALIGNRIGHT   = (1 << 1),
  MU_OPT_NOINTERACT   = (1 << 2),
  MU_OPT_NOFRAME      = (1 << 3),
  MU_OPT_NORESIZE     = (1 << 4),
  MU_OPT_NOSCROLL     = (1 << 5),
  MU_OPT_NOCLOSE      = (1 << 6),
  MU_OPT_NOTITLE      = (1 << 7),
  MU_OPT_HOLDFOCUS    = (1 << 8),
  MU_OPT_AUTOSIZE     = (1 << 9),
  MU_OPT_POPUP        = (1 << 10),
  MU_OPT_CLOSED       = (1 << 11),
  MU_OPT_EXPANDED     = (1 << 12)
};

enum {
  MU_MOUSE_LEFT       = (1 << 0),
  MU_MOUSE_RIGHT      = (1 << 1),
  MU_MOUSE_MIDDLE     = (1 << 2)
};

enum {
  MU_KEY_SHIFT        = (1 << 0),
  MU_KEY_CTRL         = (1 << 1),
  MU_KEY_ALT          = (1 << 2),
  MU_KEY_BACKSPACE    = (1 << 3),
  MU_KEY_RETURN       = (1 << 4)
};


typedef struct mu_Context mu_Context;
typedef unsigned mu_Id;
typedef MU_REAL mu_Real;
typedef void* mu_Font;

typedef struct { int x, y; } mu_Vec2;
typedef struct { int x, y, w, h; } mu_Rect;
typedef struct { unsigned char r, g, b, a; } mu_Color;
typedef struct { mu_Id id; int last_update; } mu_PoolItem;

typedef struct { int type, size; } mu_BaseCommand;
typedef struct { mu_BaseCommand base; void *dst; } mu_JumpCommand;
typedef struct { mu_BaseCommand base; mu_Rect rect; } mu_ClipCommand;
typedef struct { mu_BaseCommand base; mu_Rect rect; mu_Color color; } mu_RectCommand;
typedef struct { mu_BaseCommand base; mu_Font font; mu_Vec2 pos; mu_Color color; char str[1]; } mu_TextCommand;
typedef struct { mu_BaseCommand base; mu_Rect rect; int id; mu_Color color; } mu_IconCommand;

typedef union {
  int type;
  mu_BaseCommand base;
  mu_JumpCommand jump;
  mu_ClipCommand clip;
  mu_RectCommand rect;
  mu_TextCommand text;
  mu_IconCommand icon;
} mu_Command;

typedef struct {
  mu_Rect body;
  mu_Rect next;
  mu_Vec2 position;
  mu_Vec2 size;
  mu_Vec2 max;
  int widths[MU_MAX_WIDTHS];
  int items;
  int item_index;
  int next_row;
  int next_type;
  int indent;
} mu_Layout;

typedef struct {
  mu_Command *head, *tail;
  mu_Rect rect;
  mu_Rect body;
  mu_Vec2 content_size;
  mu_Vec2 scroll;
  int zindex;
  int open;
} mu_Container;

typedef struct {
  mu_Font font;
  mu_Vec2 size;
  int padding;
  int spacing;
  int indent;
  int title_height;
  int scrollbar_size;
  int thumb_size;
  mu_Color colors[MU_COLOR_MAX];
} mu_Style;

struct mu_Context {
  /* callbacks */
  int (*text_width)(mu_Font font, const char *str, int len);
  int (*text_height)(mu_Font font);
  void (*draw_frame)(mu_Context *ctx, mu_Rect rect, int colorid);
  /* core state */
  mu_Style _style;
  mu_Style *style;
  mu_Id hover;
  mu_Id focus;
  mu_Id last_id;
  mu_Rect last_rect;
  int last_zindex;
  int updated_focus;
  int frame;
  mu_Container *hover_root;
  mu_Container *next_hover_root;
  mu_Container *scroll_target;
  char number_edit_buf[MU_MAX_FMT];
  mu_Id number_edit;
  /* stacks */
  mu_stack(char, MU_COMMANDLIST_SIZE) command_list;
  mu_stack(mu_Container*, MU_ROOTLIST_SIZE) root_list;
  mu_stack(mu_Container*, MU_CONTAINERSTACK_SIZE) container_stack;
  mu_stack(mu_Rect, MU_CLIPSTACK_SIZE) clip_stack;
  mu_stack(mu_Id, MU_IDSTACK_SIZE) id_stack;
  mu_stack(mu_Layout, MU_LAYOUTSTACK_SIZE) layout_stack;
  /* retained state pools */
  mu_PoolItem container_pool[MU_CONTAINERPOOL_SIZE];
  mu_Container containers[MU_CONTAINERPOOL_SIZE];
  mu_PoolItem treenode_pool[MU_TREENODEPOOL_SIZE];
  /* input state */
  mu_Vec2 mouse_pos;
  mu_Vec2 last_mouse_pos;
  mu_Vec2 mouse_delta;
  mu_Vec2 scroll_delta;
  int mouse_down;
  int mouse_pressed;
  int key_down;
  int key_pressed;
  char input_text[32];
};


mu_Vec2 mu_vec2(int x, int y);
mu_Rect mu_rect(int x, int y, int w, int h);
mu_Color mu_color(int r, int g, int b, int a);

void mu_init(mu_Context *ctx);
void mu_begin(mu_Context *ctx);
void mu_end(mu_Context *ctx);
void mu_set_focus(mu_Context *ctx, mu_Id id);
mu_Id mu_get_id(mu_Context *ctx, const void *data, int size);
void mu_push_id(mu_Context *ctx, const void *data, int size);
void mu_pop_id(mu_Context *ctx);
void mu_push_clip_rect(mu_Context *ctx, mu_Rect rect);
void mu_pop_clip_rect(mu_Context *ctx);
mu_Rect mu_get_clip_rect(mu_Context *ctx);
int mu_check_clip(mu_Context *ctx, mu_Rect r);
mu_Container* mu_get_current_container(mu_Context *ctx);
mu_Container* mu_get_container(mu_Context *ctx, const char *name);
void mu_bring_to_front(mu_Context *ctx, mu_Container *cnt);

int mu_pool_init(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id);
int mu_pool_get(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id);
void mu_pool_update(mu_Context *ctx, mu_PoolItem *items, int idx);

void mu_input_mousemove(mu_Context *ctx, int x, int y);
void mu_input_mousedown(mu_Context *ctx, int x, int y, int btn);
void mu_input_mouseup(mu_Context *ctx, int x, int y, int btn);
void mu_input_scroll(mu_Context *ctx, int x, int y);
void mu_input_keydown(mu_Context *ctx, int key);
void mu_input_keyup(mu_Context *ctx, int key);
void mu_input_text(mu_Context *ctx, const char *text);

mu_Command* mu_push_command(mu_Context *ctx, int type, int size);
int mu_next_command(mu_Context *ctx, mu_Command **cmd);
void mu_set_clip(mu_Context *ctx, mu_Rect rect);
void mu_draw_rect(mu_Context *ctx, mu_Rect rect, mu_Color color);
void mu_draw_box(mu_Context *ctx, mu_Rect rect, mu_Color color);
void mu_draw_text(mu_Context *ctx, mu_Font font, const char *str, int len, mu_Vec2 pos, mu_Color color);
void mu_draw_icon(mu_Context *ctx, int id, mu_Rect rect, mu_Color color);

void mu_layout_row(mu_Context *ctx, int items, const int *widths, int height);
void mu_layout_width(mu_Context *ctx, int width);
void mu_layout_height(mu_Context *ctx, int height);
void mu_layout_begin_column(mu_Context *ctx);
void mu_layout_end_column(mu_Context *ctx);
void mu_layout_set_next(mu_Context *ctx, mu_Rect r, int relative);
mu_Rect mu_layout_next(mu_Context *ctx);

void mu_draw_control_frame(mu_Context *ctx, mu_Id id, mu_Rect rect, int colorid, int opt);
void mu_draw_control_text(mu_Context *ctx, const char *str, mu_Rect rect, int colorid, int opt);
int mu_mouse_over(mu_Context *ctx, mu_Rect rect);
void mu_update_control(mu_Context *ctx, mu_Id id, mu_Rect rect, int opt);

#define mu_button(ctx, label)             mu_button_ex(ctx, label, 0, MU_OPT_ALIGNCENTER)
#define mu_textbox(ctx, buf, bufsz)       mu_textbox_ex(ctx, buf, bufsz, 0)
#define mu_slider(ctx, value, lo, hi)     mu_slider_ex(ctx, value, lo, hi, 0, MU_SLIDER_FMT, MU_OPT_ALIGNCENTER)
#define mu_number(ctx, value, step)       mu_number_ex(ctx, value, step, MU_SLIDER_FMT, MU_OPT_ALIGNCENTER)
#define mu_header(ctx, label)             mu_header_ex(ctx, label, 0)
#define mu_begin_treenode(ctx, label)     mu_begin_treenode_ex(ctx, label, 0)
#define mu_begin_window(ctx, title, rect) mu_begin_window_ex(ctx, title, rect, 0)
#define mu_begin_panel(ctx, name)         mu_begin_panel_ex(ctx, name, 0)

void mu_text(mu_Context *ctx, const char *text);
void mu_label(mu_Context *ctx, const char *text);
int mu_button_ex(mu_Context *ctx, const char *label, int icon, int opt);
int mu_checkbox(mu_Context *ctx, const char *label, int *state);
int mu_textbox_raw(mu_Context *ctx, char *buf, int bufsz, mu_Id id, mu_Rect r, int opt);
int mu_textbox_ex(mu_Context *ctx, char *buf, int bufsz, int opt);
int mu_slider_ex(mu_Context *ctx, mu_Real *value, mu_Real low, mu_Real high, mu_Real step, const char *fmt, int opt);
int mu_number_ex(mu_Context *ctx, mu_Real *value, mu_Real step, const char *fmt, int opt);
int mu_header_ex(mu_Context *ctx, const char *label, int opt);
int mu_begin_treenode_ex(mu_Context *ctx, const char *label, int opt);
void mu_end_treenode(mu_Context *ctx);
int mu_begin_window_ex(mu_Context *ctx, const char *title, mu_Rect rect, int opt);
void mu_end_window(mu_Context *ctx);
void mu_open_popup(mu_Context *ctx, const char *name);
int mu_begin_popup(mu_Context *ctx, const char *name);
void mu_end_popup(mu_Context *ctx);
void mu_begin_panel_ex(mu_Context *ctx, const char *name, int opt);
void mu_end_panel(mu_Context *ctx);

#endif
