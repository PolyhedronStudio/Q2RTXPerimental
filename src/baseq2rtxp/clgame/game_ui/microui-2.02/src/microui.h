/*
** Copyright (c) 2024 rxi
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the MIT license. See `microui.c` for details.
*/
/********************************************************************
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
//! The maximum number of items that can be clipped on a container's layout stack. This is used for nested layouts within a container.
#define MU_CLIPSTACK_SIZE       32
//! The maximum number of IDs that can be in the ID stack. This is used for generating unique IDs for UI elements.
#define MU_IDSTACK_SIZE         32
//! The maximum number of layouts that can be in the layout stack. This is used for nested layouts within a container.
#define MU_LAYOUTSTACK_SIZE     16
//! The maximum number of items that can be in a container's retained pool. This caps the number of live containers tracked at once.
#define MU_CONTAINERPOOL_SIZE   48
//! The maximum number of items that can be in the tree-node retained pool. This caps the number of remembered treenode states.
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
*	@brief	Macro that defines a compact stack structure for a given type and size.
*	@note	Used internally for the command, root, clip, ID, and layout stacks.
**/
//! Example: `mu_stack( int, 16 )` creates a stack with an index and 16 items.
#define mu_stack(T, n)          struct { int idx; T items[n]; }
//! Minimum macro, returns the smaller of a and b. This is used for clamping values and other operations where we need to ensure a value does not exceed a certain limit.
#define mu_min(a, b)            ((a) < (b) ? (a) : (b))
//! Maximum macro, returns the larger of a and b. This is used for clamping values and other operations where we need to ensure a value does not go below a certain limit.
#define mu_max(a, b)            ((a) > (b) ? (a) : (b))
//! Clamp macro, returns x clamped between a and b. This is used for ensuring values stay within a certain range, such as when scrolling or resizing UI elements.
#define mu_clamp(x, a, b)       mu_min(b, mu_max(a, x))

/**
*	@brief	Clip mode for clipping commands.
*	@note	`MU_CLIP_PART` clips the current container only, while `MU_CLIP_ALL` skips the entire command.
**/
enum {
	//! Clip mode that keeps partially visible content and emits a local clip rectangle.
	MU_CLIP_PART = 1,
	//! Clip mode that treats the rectangle as fully clipped and skips the draw command.
	MU_CLIP_ALL
};

/**
*	@details	The UI builds a packed command list (`mu_Command` entries).
*			Each command starts with a `mu_BaseCommand` header (type + size).
*			These enum values identify the concrete payload that follows.
**/
enum {
	//! Jump/branch command used to alter the current command pointer.
	MU_COMMAND_JUMP = 1,
	//! Clipping command that sets the current clip rectangle for subsequent draw commands.
	MU_COMMAND_CLIP,
	//! Draw-rect command for filled rectangles.
	MU_COMMAND_RECT,
	//! Draw-text command containing font, position, color, and trailing string data.
	MU_COMMAND_TEXT,
	//! Draw-icon command containing an icon id, destination rectangle, and color.
	MU_COMMAND_ICON,
	//! Sentinel value one past the last valid command type.
	MU_COMMAND_MAX
};

/**
*	@brief	Theme color slots used by `mu_Style::colors`.
*	@note	Indices are stable API values used by widgets and custom draw hooks.
**/
enum {
	//! Default text color.
	MU_COLOR_TEXT,
	//! Border and outline color.
	MU_COLOR_BORDER,
	//! Window background color.
	MU_COLOR_WINDOWBG,
	//! Title bar background color when window is active.
	MU_COLOR_TITLEBG_ACTIVE,
	//! Title bar background color when the window is inactive.
	MU_COLOR_TITLEBG_INACTIVE,
	//! Title bar text color.
	MU_COLOR_TITLETEXT,
	//! Panel background color.
	MU_COLOR_PANELBG,
	//! Button fill color.
	MU_COLOR_BUTTON,
	//! Button fill color while hovered.
	MU_COLOR_BUTTONHOVER,
	//! Button fill color while focused/active.
	MU_COLOR_BUTTONFOCUS,
	//! Base control fill color.
	MU_COLOR_BASE,
	//! Base control fill color while hovered.
	MU_COLOR_BASEHOVER,
	//! Base control fill color while focused/active.
	MU_COLOR_BASEFOCUS,
	//! Scrollbar track color.
	MU_COLOR_SCROLLBASE,
	//! Scrollbar thumb color.
	MU_COLOR_SCROLLTHUMB,
	//! Number of color slots.
	MU_COLOR_MAX
};

/**
*	@brief	Built-in icon identifiers for standard controls.
**/
enum {
	//! Close icon.
	MU_ICON_CLOSE = 1,
	//! Checkmark icon.
	MU_ICON_CHECK,
	//! Collapsed tree-node icon.
	MU_ICON_COLLAPSED,
	//! Expanded tree-node icon.
	MU_ICON_EXPANDED,
	//! Number of icon slots.
	MU_ICON_MAX
};

/**
*	@brief	Widget result flags returned by interactive controls.
*	@note	Flags can be combined with bitwise checks.
**/
enum {
	//! Widget is currently active.
	MU_RES_ACTIVE       = (1 << 0),
	//! Widget was submitted this frame.
	MU_RES_SUBMIT       = (1 << 1),
	//! Widget changed value or state.
	MU_RES_CHANGE       = (1 << 2)
};

/**
*	@brief	Widget and container option flags.
*	@note	These alter alignment, interaction, frame/title visibility, popup behavior, and open/closed state.
**/
enum {
	//! Center widget contents horizontally.
	MU_OPT_ALIGNCENTER  = (1 << 0),
	//! Align widget contents to the right.
	MU_OPT_ALIGNRIGHT   = (1 << 1),
	//! Disable interaction for this control.
	MU_OPT_NOINTERACT   = (1 << 2),
	//! Hide the frame decoration.
	MU_OPT_NOFRAME      = (1 << 3),
	//! Disable resize handles.
	MU_OPT_NORESIZE     = (1 << 4),
	//! Disable scrollbars.
	MU_OPT_NOSCROLL     = (1 << 5),
	//! Prevent a window from being closed.
	MU_OPT_NOCLOSE      = (1 << 6),
	//! Hide the title bar.
	MU_OPT_NOTITLE      = (1 << 7),
	//! Keep focus until explicitly cleared.
	MU_OPT_HOLDFOCUS    = (1 << 8),
	//! Auto-size the window to content.
	MU_OPT_AUTOSIZE     = (1 << 9),
	//! Mark the window as a popup.
	MU_OPT_POPUP        = (1 << 10),
	//! Start a container in the closed state.
	MU_OPT_CLOSED       = (1 << 11),
	//! Force a tree node or header to start expanded.
	MU_OPT_EXPANDED     = (1 << 12),
	//! Prevent focus from being lost when clicking outside the window or on another window.
	MU_OPT_KEEPFOCUS = ( 1 << 13 ),
	//! Prevent dragging the window when clicking on the title bar.
	MU_OPT_NODRAG = ( 1 << 14 ),
};

/**
*	@brief	Mouse button bitmask flags consumed by input functions.
**/
enum {
	//! Left mouse button.
	MU_MOUSE_LEFT       = (1 << 0),
	//! Right mouse button.
	MU_MOUSE_RIGHT      = (1 << 1),
	//! Middle mouse button.
	MU_MOUSE_MIDDLE     = (1 << 2)
};

/**
*	@brief	Keyboard modifier and special-key bitmask flags.
**/
enum {
	//! Shift key.
	MU_KEY_SHIFT        = (1 << 0),
	//! Control key.
	MU_KEY_CTRL         = (1 << 1),
	//! Alt key.
	MU_KEY_ALT          = (1 << 2),
	//! Backspace key.
	MU_KEY_BACKSPACE    = (1 << 3),
	//! Return/enter key.
	MU_KEY_RETURN       = (1 << 4)
};


/**
*	@brief	Forward declaration of the primary UI context.
**/
typedef struct mu_Context mu_Context;
//! Stable widget identifier type.
typedef unsigned mu_Id;
//! Real-number type used for slider and numeric controls.
typedef MU_REAL mu_Real;
//! Opaque font handle supplied by the host application.
typedef void* mu_Font;

/**
*	@brief	2D integer vector used for positions and dimensions.
**/
typedef struct {
	//! Horizontal component.
	int x;
	//! Vertical component.
	int y;
} mu_Vec2;

/**
*	@brief	Rectangle in integer pixel space.
**/
typedef struct {
	//! Left coordinate.
	int x;
	//! Top coordinate.
	int y;
	//! Width.
	int w;
	//! Height.
	int h;
} mu_Rect;

/**
*	@brief	RGBA color with 8-bit channels.
**/
typedef struct {
	//! Red channel.
	unsigned char r;
	//! Green channel.
	unsigned char g;
	//! Blue channel.
	unsigned char b;
	//! Alpha channel.
	unsigned char a;
} mu_Color;

/**
*	@brief	Retained-state pool entry for ID aging and reuse.
**/
typedef struct {
	//! Cached identifier for the pooled slot.
	mu_Id id;
	//! Frame index of the last update.
	int last_update;
} mu_PoolItem;

/**
*	@brief	Common command header stored at the start of all command payloads.
**/
typedef struct {
	//! Command type tag.
	int type;
	//! Total command size in bytes.
	int size;
} mu_BaseCommand;

/**
*	@brief	Command payload for command-stream pointer jumps.
**/
typedef struct {
	//! Common command header.
	mu_BaseCommand base;
	//! Destination command pointer.
	void *dst;
} mu_JumpCommand;

/**
*	@brief	Command payload that sets the active clip rectangle.
**/
typedef struct {
	//! Common command header.
	mu_BaseCommand base;
	//! Clip rectangle.
	mu_Rect rect;
} mu_ClipCommand;

/**
*	@brief	Command payload for filled rectangle drawing.
**/
typedef struct {
	//! Common command header.
	mu_BaseCommand base;
	//! Rectangle to draw.
	mu_Rect rect;
	//! Fill color.
	mu_Color color;
} mu_RectCommand;

/**
*	@brief	Command payload for text drawing.
*	@note	`str` is a flexible trailing storage area encoded directly in the command stream.
**/
typedef struct {
	//! Common command header.
	mu_BaseCommand base;
	//! Font handle.
	mu_Font font;
	//! Text position.
	mu_Vec2 pos;
	//! Text color.
	mu_Color color;
	//! Trailing string storage.
	char str[1];
} mu_TextCommand;

/**
*	@brief	Command payload for icon drawing.
**/
typedef struct {
	//! Common command header.
	mu_BaseCommand base;
	//! Destination rectangle.
	mu_Rect rect;
	//! Icon identifier.
	int id;
	//! Tint color.
	mu_Color color;
} mu_IconCommand;

/**
*	@brief	Tagged view over any command payload in the command stream.
**/
typedef union {
	//! Raw command type.
	int type;
	//! Common command header view.
	mu_BaseCommand base;
	//! Jump command view.
	mu_JumpCommand jump;
	//! Clip command view.
	mu_ClipCommand clip;
	//! Rectangle command view.
	mu_RectCommand rect;
	//! Text command view.
	mu_TextCommand text;
	//! Icon command view.
	mu_IconCommand icon;
} mu_Command;

/**
*	@brief	Per-container layout cursor and row/column state.
**/
typedef struct {
	//! Current content body rectangle.
	mu_Rect body;
	//! Deferred next-item rectangle.
	mu_Rect next;
	//! Current layout position.
	mu_Vec2 position;
	//! Current item size.
	mu_Vec2 size;
	//! Running maximum extents.
	mu_Vec2 max;
	//! Explicit per-item widths.
	int widths[MU_MAX_WIDTHS];
	//! Number of items in the current row.
	int items;
	//! Current item index within the row.
	int item_index;
	//! Y position for the next row.
	int next_row;
	//! Deferred next-item type.
	int next_type;
	//! Indentation applied to the row.
	int indent;
} mu_Layout;

/**
*	@brief	Window or panel state and linked command range for a container.
**/
typedef struct {
	//! First command in the container's command chain.
	mu_Command *head;
	//! Final command in the container's command chain.
	mu_Command *tail;
	//! Outer container rectangle.
	mu_Rect rect;
	//! Inner content body rectangle.
	mu_Rect body;
	//! Accumulated content size.
	mu_Vec2 content_size;
	//! Scroll offsets.
	mu_Vec2 scroll;
	//! Z-order used for sorting containers.
	int zindex;
	//! Whether the container is open.
	int open;
} mu_Container;

/**
*	@brief	Theme/style configuration used by all widgets.
**/
typedef struct {
	//! Default font.
	mu_Font font;
	//! Base size used for default controls.
	mu_Vec2 size;
	//! Default padding inside controls.
	int padding;
	//! Spacing between widgets.
	int spacing;
	//! Indentation step used by tree nodes and sublayouts.
	int indent;
	//! Title bar height.
	int title_height;
	//! Scrollbar thickness.
	int scrollbar_size;
	//! Scroll thumb minimum size.
	int thumb_size;
	//! Theme color table.
	mu_Color colors[MU_COLOR_MAX];
} mu_Style;

/**
*	@brief	Main UI context containing callbacks, retained state, stacks, and input state.
*	@note	One context instance typically represents one complete immediate-mode UI state machine.
**/
struct mu_Context {
	/* callbacks */
	//! Function used to measure text width.
	int (*text_width)(mu_Font font, const char *str, int len);
	//! Function used to measure text height.
	int (*text_height)(mu_Font font);
	//! Function used to draw a frame for a control.
	void (*draw_frame)(mu_Context *ctx, mu_Rect rect, int colorid);
	/* core state */
	//! Inline default style storage.
	mu_Style _style;
	//! Active style pointer.
	mu_Style *style;
	//! Current hover widget ID.
	mu_Id hover;
	//! Current keyboard focus widget ID.
	mu_Id focus;
	//! Last generated widget ID.
	mu_Id last_id;
	//! Last rectangle produced by layout.
	mu_Rect last_rect;
	//! Highest z-index seen so far.
	int last_zindex;
	//! Whether focus was refreshed this frame.
	int updated_focus;
	//! Frame counter.
	int frame;
	//! Current hover root container.
	mu_Container *hover_root;
	//! Candidate hover root container for this frame.
	mu_Container *next_hover_root;
	//! Container currently receiving scroll input.
	mu_Container *scroll_target;
	//! Temporary buffer for number/text editing.
	char number_edit_buf[MU_MAX_FMT];
	//! Currently edited numeric widget ID.
	mu_Id number_edit;
	/* stacks */
	//! Packed command buffer.
	mu_stack(char, MU_COMMANDLIST_SIZE) command_list;
	//! Root container stack.
	mu_stack(mu_Container*, MU_ROOTLIST_SIZE) root_list;
	//! Active container stack.
	mu_stack(mu_Container*, MU_CONTAINERSTACK_SIZE) container_stack;
	//! Clip rectangle stack.
	mu_stack(mu_Rect, MU_CLIPSTACK_SIZE) clip_stack;
	//! ID seed stack.
	mu_stack(mu_Id, MU_IDSTACK_SIZE) id_stack;
	//! Layout state stack.
	mu_stack(mu_Layout, MU_LAYOUTSTACK_SIZE) layout_stack;
	/* retained state pools */
	//! Pool of retained container metadata.
	mu_PoolItem container_pool[MU_CONTAINERPOOL_SIZE];
	//! Retained container instances.
	mu_Container containers[MU_CONTAINERPOOL_SIZE];
	//! Pool of retained tree-node metadata.
	mu_PoolItem treenode_pool[MU_TREENODEPOOL_SIZE];
	/* input state */
	//! Current mouse position.
	mu_Vec2 mouse_pos;
	//! Previous mouse position.
	mu_Vec2 last_mouse_pos;
	//! Per-frame mouse movement delta.
	mu_Vec2 mouse_delta;
	//! Per-frame scroll delta.
	mu_Vec2 scroll_delta;
	//! Bitmask of held mouse buttons.
	int mouse_down;
	//! Bitmask of pressed mouse buttons this frame.
	int mouse_pressed;
	//! Bitmask of held keys.
	int key_down;
	//! Bitmask of pressed keys this frame.
	int key_pressed;
	//! Buffered UTF-8 input text.
	char input_text[32];
};


/**
*	@brief	Construct a 2D integer vector.
*	@param x Horizontal component.
*	@param y Vertical component.
*	@return A populated `mu_Vec2`.
**/
mu_Vec2 mu_vec2(int x, int y);

/**
*	@brief	Construct an integer rectangle.
*	@param x Left coordinate.
*	@param y Top coordinate.
*	@param w Width.
*	@param h Height.
*	@return A populated `mu_Rect`.
**/
mu_Rect mu_rect(int x, int y, int w, int h);

/**
*	@brief	Construct an RGBA color.
*	@param r Red channel.
*	@param g Green channel.
*	@param b Blue channel.
*	@param a Alpha channel.
*	@return A populated `mu_Color`.
**/
mu_Color mu_color(int r, int g, int b, int a);

/**
*	@brief	Initialize a context and default style/state.
*	@param ctx Context to initialize.
**/
void mu_init(mu_Context *ctx);

/**
*	@brief	Begin a new UI frame.
*	@param ctx Active context.
**/
void mu_begin(mu_Context *ctx);

/**
*	@brief	End a UI frame and finalize command output.
*	@param ctx Active context.
**/
void mu_end(mu_Context *ctx);

/**
*	@brief	Set keyboard focus to a specific widget ID.
*	@param ctx Active context.
*	@param id Widget identifier to focus.
**/
void mu_set_focus(mu_Context *ctx, mu_Id id);

/**
*	@brief	Hash arbitrary bytes with the current ID stack to produce a stable widget ID.
*	@param ctx Active context.
*	@param data Seed data used to derive the ID.
*	@param size Size of the seed data in bytes.
*	@return A stable widget ID.
**/
mu_Id mu_get_id(mu_Context *ctx, const void *data, int size);

/**
*	@brief	Push a scope ID seed for nested ID generation.
*	@param ctx Active context.
*	@param data Seed data.
*	@param size Size of the seed data in bytes.
**/
void mu_push_id(mu_Context *ctx, const void *data, int size);

/**
*	@brief	Pop the most recent scope ID seed.
*	@param ctx Active context.
**/
void mu_pop_id(mu_Context *ctx);

/**
*	@brief	Push and intersect a new clipping rectangle.
*	@param ctx Active context.
*	@param rect Rectangle to intersect with the current clip region.
**/
void mu_push_clip_rect(mu_Context *ctx, mu_Rect rect);

/**
*	@brief	Restore the previous clipping rectangle.
*	@param ctx Active context.
**/
void mu_pop_clip_rect(mu_Context *ctx);

/**
*	@brief	Get the current effective clipping rectangle.
*	@param ctx Active context.
*	@return The active clip rectangle.
**/
mu_Rect mu_get_clip_rect(mu_Context *ctx);

/**
*	@brief	Test a rectangle against the current clip region.
*	@param ctx Active context.
*	@param r Rectangle to test.
*	@return A clip mode value.
**/
int mu_check_clip(mu_Context *ctx, mu_Rect r);

/**
*	@brief	Get the current container at the top of the container stack.
*	@param ctx Active context.
*	@return The current container.
**/
mu_Container* mu_get_current_container(mu_Context *ctx);

/**
*	@brief	Fetch or create a retained container by name.
*	@param ctx Active context.
*	@param name Container name key.
*	@return The retained container instance.
**/
mu_Container* mu_get_container(mu_Context *ctx, const char *name);

/**
*	@brief	Move a container to top-most z-order.
*	@param ctx Active context.
*	@param cnt Container to raise.
**/
void mu_bring_to_front(mu_Context *ctx, mu_Container *cnt);

/**
*	@brief	Initialize a retained pool by evicting the least-recently updated entry.
*	@param ctx Active context.
*	@param items Pool entries.
*	@param len Pool length.
*	@param id Identifier to store.
*	@return Index of the allocated slot.
**/
int mu_pool_init(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id);

/**
*	@brief	Look up a retained pool item by ID.
*	@param ctx Active context.
*	@param items Pool entries.
*	@param len Pool length.
*	@param id Identifier to search for.
*	@return Index of the matching slot or `-1`.
**/
int mu_pool_get(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id);

/**
*	@brief	Refresh the update time for a pool entry.
*	@param ctx Active context.
*	@param items Pool entries.
*	@param idx Slot index to refresh.
**/
void mu_pool_update(mu_Context *ctx, mu_PoolItem *items, int idx);

/**
*	@brief	Update the current mouse position.
*	@param ctx Active context.
*	@param x Mouse x coordinate.
*	@param y Mouse y coordinate.
**/
void mu_input_mousemove(mu_Context *ctx, int x, int y);

/**
*	@brief	Register a mouse-button press.
*	@param ctx Active context.
*	@param x Mouse x coordinate.
*	@param y Mouse y coordinate.
*	@param btn Mouse button bitmask.
**/
void mu_input_mousedown(mu_Context *ctx, int x, int y, int btn);

/**
*	@brief	Register a mouse-button release.
*	@param ctx Active context.
*	@param x Mouse x coordinate.
*	@param y Mouse y coordinate.
*	@param btn Mouse button bitmask.
**/
void mu_input_mouseup(mu_Context *ctx, int x, int y, int btn);

/**
*	@brief	Accumulate mouse-wheel scrolling.
*	@param ctx Active context.
*	@param x Horizontal scroll delta.
*	@param y Vertical scroll delta.
**/
void mu_input_scroll(mu_Context *ctx, int x, int y);

/**
*	@brief	Register a key press.
*	@param ctx Active context.
*	@param key Key bitmask.
**/
void mu_input_keydown(mu_Context *ctx, int key);

/**
*	@brief	Register a key release.
*	@param ctx Active context.
*	@param key Key bitmask.
**/
void mu_input_keyup(mu_Context *ctx, int key);

/**
*	@brief	Append UTF-8 input text to the frame buffer.
*	@param ctx Active context.
*	@param text UTF-8 text to append.
**/
void mu_input_text(mu_Context *ctx, const char *text);

/**
*	@brief	Push a command payload onto the current command list.
*	@param ctx Active context.
*	@param type Command type tag.
*	@param size Command size in bytes.
*	@return Pointer to the new command.
**/
mu_Command* mu_push_command(mu_Context *ctx, int type, int size);

/**
*	@brief	Advance to the next executable command in the command list.
*	@param ctx Active context.
*	@param cmd Current iterator pointer.
*	@return Non-zero if a command was produced.
**/
int mu_next_command(mu_Context *ctx, mu_Command **cmd);

/**
*	@brief	Emit a clip command.
*	@param ctx Active context.
*	@param rect Clip rectangle.
**/
void mu_set_clip(mu_Context *ctx, mu_Rect rect);

/**
*	@brief	Emit a filled rectangle draw command.
*	@param ctx Active context.
*	@param rect Rectangle to draw.
*	@param color Fill color.
**/
void mu_draw_rect(mu_Context *ctx, mu_Rect rect, mu_Color color);

/**
*	@brief	Emit a 1-pixel box outline using four rectangles.
*	@param ctx Active context.
*	@param rect Box rectangle.
*	@param color Outline color.
**/
void mu_draw_box(mu_Context *ctx, mu_Rect rect, mu_Color color);

/**
*	@brief	Emit a text draw command.
*	@param ctx Active context.
*	@param font Font handle.
*	@param str UTF-8 string.
*	@param len String length or `-1` for null-terminated input.
*	@param pos Text position.
*	@param color Text color.
**/
void mu_draw_text(mu_Context *ctx, mu_Font font, const char *str, int len, mu_Vec2 pos, mu_Color color);

/**
*	@brief	Emit an icon draw command.
*	@param ctx Active context.
*	@param id Icon identifier.
*	@param rect Destination rectangle.
*	@param color Tint color.
**/
void mu_draw_icon(mu_Context *ctx, int id, mu_Rect rect, mu_Color color);

/**
*	@brief	Configure the current layout row.
*	@param ctx Active context.
*	@param items Number of items in the row.
*	@param widths Per-item widths, or `NULL` for automatic sizing.
*	@param height Row height.
**/
void mu_layout_row(mu_Context *ctx, int items, const int *widths, int height);

/**
*	@brief	Override the next item width.
*	@param ctx Active context.
*	@param width Desired width.
**/
void mu_layout_width(mu_Context *ctx, int width);

/**
*	@brief	Override the next item height.
*	@param ctx Active context.
*	@param height Desired height.
**/
void mu_layout_height(mu_Context *ctx, int height);

/**
*	@brief	Begin a nested column layout.
*	@param ctx Active context.
**/
void mu_layout_begin_column(mu_Context *ctx);

/**
*	@brief	End the current nested column layout.
*	@param ctx Active context.
**/
void mu_layout_end_column(mu_Context *ctx);

/**
*	@brief	Set the next layout rectangle.
*	@param ctx Active context.
*	@param r Rectangle to use for the next item.
*	@param relative Non-zero to treat the rectangle as relative to the body.
**/
void mu_layout_set_next(mu_Context *ctx, mu_Rect r, int relative);

/**
*	@brief	Advance the layout and return the next item rectangle.
*	@param ctx Active context.
*	@return The next layout rectangle.
**/
mu_Rect mu_layout_next(mu_Context *ctx);

/**
*	@brief	Draw a control frame using the state-dependent theme color.
*	@param ctx Active context.
*	@param id Widget identifier.
*	@param rect Control rectangle.
*	@param colorid Base color index.
*	@param opt Control option bitmask.
**/
void mu_draw_control_frame(mu_Context *ctx, mu_Id id, mu_Rect rect, int colorid, int opt);

/**
*	@brief	Draw control text clipped to a rectangle.
*	@param ctx Active context.
*	@param str Text string.
*	@param rect Control rectangle.
*	@param colorid Theme color index.
*	@param opt Control option bitmask.
**/
void mu_draw_control_text(mu_Context *ctx, const char *str, mu_Rect rect, int colorid, int opt);
void mu_draw_control_text_custom_color( mu_Context *ctx, const char *str, mu_Rect rect,
	mu_Color color, int opt );

/**
*	@brief	Test whether the mouse is over a rectangle and the active hover root.
*	@param ctx Active context.
*	@param rect Rectangle to test.
*	@return Non-zero when the rectangle is under the cursor.
**/
int mu_mouse_over(mu_Context *ctx, mu_Rect rect);

/**
*	@brief	Update hover and focus state for a control.
*	@param ctx Active context.
*	@param id Widget identifier.
*	@param rect Control rectangle.
*	@param opt Control option bitmask.
**/
void mu_update_control(mu_Context *ctx, mu_Id id, mu_Rect rect, int opt);

#define mu_button(ctx, label)             mu_button_ex(ctx, label, 0, MU_OPT_ALIGNCENTER)
#define mu_textbox(ctx, buf, bufsz)       mu_textbox_ex(ctx, buf, bufsz, 0)
#define mu_slider(ctx, value, lo, hi)     mu_slider_ex(ctx, value, lo, hi, 0, MU_SLIDER_FMT, MU_OPT_ALIGNCENTER)
#define mu_number(ctx, value, step)       mu_number_ex(ctx, value, step, MU_SLIDER_FMT, MU_OPT_ALIGNCENTER)
#define mu_header(ctx, label)             mu_header_ex(ctx, label, 0)
#define mu_begin_treenode(ctx, label)     mu_begin_treenode_ex(ctx, label, 0)
#define mu_begin_window(ctx, title, rect) mu_begin_window_ex(ctx, title, rect, 0)
#define mu_begin_panel(ctx, name)         mu_begin_panel_ex(ctx, name, 0)

/**
*	@brief	Emit static text using the current layout.
*	@param ctx Active context.
*	@param text UTF-8 text to draw.
**/
void mu_text(mu_Context *ctx, const char *text);

/**
*	@brief	Emit a single-line label using the current layout.
*	@param ctx Active context.
*	@param text Label text.
**/
void mu_label(mu_Context *ctx, const char *text);
/**
* 	@brief	Emit a single-line label using the current layout.
* 	@param	ctx		Active context.
* 	@param	text	Label text.
* 	@param	colorid	The color to use.
* 	@param	opt		Special options for this label.
**/
void mu_label_ex( mu_Context *ctx, const char *text, const int colorid, const int opt );
/**
*	@brief	Emit a button widget.
*	@param ctx Active context.
*	@param label Button label, or `NULL` when using an icon.
*	@param icon Icon identifier.
*	@param opt Control option bitmask.
*	@return Widget result flags.
**/
int mu_button_ex(mu_Context *ctx, const char *label, int icon, int opt);

/**
*	@brief	Emit a checkbox widget.
*	@param ctx Active context.
*	@param label Checkbox label.
*	@param state Checkbox state pointer.
*	@return Widget result flags.
**/
int mu_checkbox(mu_Context *ctx, const char *label, int *state);

/**
*	@brief	Edit a raw text buffer in-place.
*	@param ctx Active context.
*	@param buf Editable buffer.
*	@param bufsz Buffer size.
*	@param id Widget identifier.
*	@param r Control rectangle.
*	@param opt Control option bitmask.
*	@return Widget result flags.
**/
int mu_textbox_raw(mu_Context *ctx, char *buf, int bufsz, mu_Id id, mu_Rect r, int opt);

/**
*	@brief	Emit a standard text box widget.
*	@param ctx Active context.
*	@param buf Editable buffer.
*	@param bufsz Buffer size.
*	@param opt Control option bitmask.
*	@return Widget result flags.
**/
int mu_textbox_ex(mu_Context *ctx, char *buf, int bufsz, int opt);

/**
*	@brief	Emit a slider widget.
*	@param ctx Active context.
*	@param value Slider value pointer.
*	@param low Minimum value.
*	@param high Maximum value.
*	@param step Quantization step.
*	@param fmt Display format.
*	@param opt Control option bitmask.
*	@return Widget result flags.
**/
int mu_slider_ex(mu_Context *ctx, mu_Real *value, mu_Real low, mu_Real high, mu_Real step, const char *fmt, int opt);

/**
*	@brief	Emit a numeric entry widget.
*	@param ctx Active context.
*	@param value Numeric value pointer.
*	@param step Dragging step size.
*	@param fmt Display format.
*	@param opt Control option bitmask.
*	@return Widget result flags.
**/
int mu_number_ex(mu_Context *ctx, mu_Real *value, mu_Real step, const char *fmt, int opt);

/**
*	@brief	Emit a header widget.
*	@param ctx Active context.
*	@param label Header label.
*	@param opt Control option bitmask.
*	@return Widget result flags.
**/
int mu_header_ex(mu_Context *ctx, const char *label, int opt);

/**
*	@brief	Emit a tree-node header widget.
*	@param ctx Active context.
*	@param label Node label.
*	@param opt Control option bitmask.
*	@return Widget result flags.
**/
int mu_begin_treenode_ex(mu_Context *ctx, const char *label, int opt);

/**
*	@brief	End the current tree node scope.
*	@param ctx Active context.
**/
void mu_end_treenode(mu_Context *ctx);

/**
*	@brief	Emit a standard window widget.
*	@param ctx Active context.
*	@param title Window title.
*	@param rect Initial window rectangle.
*	@param opt Window option bitmask.
*	@return Widget result flags.
**/
int mu_begin_window_ex(mu_Context *ctx, const char *title, mu_Rect rect, int opt);

/**
*	@brief	End the current window scope.
*	@param ctx Active context.
**/
void mu_end_window(mu_Context *ctx);

/**
*	@brief	Open a named popup at the mouse cursor.
*	@param ctx Active context.
*	@param name Popup name.
**/
void mu_open_popup(mu_Context *ctx, const char *name);

/**
*	@brief	Begin a popup window with standard popup options.
*	@param ctx Active context.
*	@param name Popup name.
*	@return Widget result flags.
**/
int mu_begin_popup(mu_Context *ctx, const char *name);

/**
*	@brief	End the current popup scope.
*	@param ctx Active context.
**/
void mu_end_popup(mu_Context *ctx);

/**
*	@brief	Begin a retained panel widget.
*	@param ctx Active context.
*	@param name Panel name.
*	@param opt Panel option bitmask.
**/
void mu_begin_panel_ex(mu_Context *ctx, const char *name, int opt);

/**
*	@brief	End the current panel scope.
*	@param ctx Active context.
**/
void mu_end_panel(mu_Context *ctx);

#endif
