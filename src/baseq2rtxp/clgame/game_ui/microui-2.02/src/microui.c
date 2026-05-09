/*
** Copyright (c) 2024 rxi
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to
** deal in the Software without restriction, including without limitation the
** rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
** sell copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
** IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "microui.h"

#define unused(x) ((void) (x))

#define expect(x) do {                                               \
    if (!(x)) {                                                      \
      fprintf(stderr, "Fatal error: %s:%d: assertion '%s' failed\n", \
        __FILE__, __LINE__, #x);                                     \
      abort();                                                       \
    }                                                                \
  } while (0)

#define push(stk, val) do {                                                 \
    expect((stk).idx < (int) (sizeof((stk).items) / sizeof(*(stk).items))); \
    (stk).items[(stk).idx] = (val);                                         \
    (stk).idx++; /* incremented after incase `val` uses this value */       \
  } while (0)

#define pop(stk) do {      \
    expect((stk).idx > 0); \
    (stk).idx--;           \
  } while (0)


static mu_Rect unclipped_rect = { 0, 0, 0x1000000, 0x1000000 };

static mu_Style default_style = {
  /* font | size | padding | spacing | indent */
  NULL, { 34, 4 }, 10, 4, 12/*24*/,
  /* title_height | scrollbar_size | thumb_size */
  CHAR_HEIGHT + 4, 12, 8,
  {
	// <Q2RTXP>: WID: Default old initial theme.
    //{ 230, 230, 230, 255 }, /* MU_COLOR_TEXT */
    //{ 25,  25,  25,  255 }, /* MU_COLOR_BORDER */
    //{ 50,  50,  50,  255 }, /* MU_COLOR_WINDOWBG */
    //{ 25,  25,  25,  255 }, /* MU_COLOR_TITLEBG */
    //{ 240, 240, 240, 255 }, /* MU_COLOR_TITLETEXT */
    //{ 0,   0,   0,   0   }, /* MU_COLOR_PANELBG */
    //{ 75,  75,  75,  255 }, /* MU_COLOR_BUTTON */
    //{ 95,  95,  95,  255 }, /* MU_COLOR_BUTTONHOVER */
    //{ 115, 115, 115, 255 }, /* MU_COLOR_BUTTONFOCUS */
    //{ 30,  30,  30,  255 }, /* MU_COLOR_BASE */
    //{ 35,  35,  35,  255 }, /* MU_COLOR_BASEHOVER */
    //{ 40,  40,  40,  255 }, /* MU_COLOR_BASEFOCUS */
    //{ 43,  43,  43,  255 }, /* MU_COLOR_SCROLLBASE */
    //{ 30,  30,  30,  255 }  /* MU_COLOR_SCROLLTHUMB */

	// <Q2RTXP>: WID: Our own custom theme! :-)
	{ 230, 230, 230, 255 }, /* MU_COLOR_TEXT */
	{ 255, 144,  90, 235 }, /* MU_COLOR_BORDER */
	{  40,  40,  40, 240 }, /* MU_COLOR_WINDOWBG */
	{ 255, 120,   0, 255 }, /* MU_COLOR_TITLEBG_ACTIVE */ //! This would be for the `ACTIVE` title bar rendering instead.
	{ 255, 100,   0, 128 }, /* MU_COLOR_TITLEBG_INACTIVE */ //! This would be for the `IN-ACTIVE` title bar rendering instead.
	{ 255, 255, 255, 255 }, /* MU_COLOR_TITLETEXT */
	{   0,   0,   0,   0 }, /* MU_COLOR_PANELBG */
	{ 255, 145,  90, 156 }, /* MU_COLOR_BUTTON */
	{ 255, 147,  54, 210 }, /* MU_COLOR_BUTTONHOVER */
	{ 255, 120,  48, 210 }, /* MU_COLOR_BUTTONFOCUS */
	{ 255, 255, 255,  58 }, /* MU_COLOR_BASE */
	{ 255, 126, 90,   44 }, /* MU_COLOR_BASEHOVER */
	{ 165, 118, 76,  110 }, /* MU_COLOR_BASEFOCUS */
	{ 170, 110, 58,  160 }, /* MU_COLOR_SCROLLBASE */
	{ 255, 120,  0,  156 }  /* MU_COLOR_SCROLLTHUMB */
  }
};


/**
*	@brief	Construct a 2D integer vector.
*	@param x Horizontal component.
*	@param y Vertical component.
*	@return A populated `mu_Vec2`.
*/
mu_Vec2 mu_vec2(int x, int y) {
  mu_Vec2 res;
  res.x = x; res.y = y;
  return res;
}


/**
*	@brief	Construct an integer rectangle.
*	@param x Left coordinate.
*	@param y Top coordinate.
*	@param w Width.
*	@param h Height.
*	@return A populated `mu_Rect`.
*/
mu_Rect mu_rect(int x, int y, int w, int h) {
  mu_Rect res;
  res.x = x; res.y = y; res.w = w; res.h = h;
  return res;
}


/**
*	@brief	Construct an RGBA color.
*	@param r Red channel.
*	@param g Green channel.
*	@param b Blue channel.
*	@param a Alpha channel.
*	@return A populated `mu_Color`.
*/
mu_Color mu_color(int r, int g, int b, int a) {
  mu_Color res;
  res.r = r; res.g = g; res.b = b; res.a = a;
  return res;
}


/**
*	@brief	Expand a rectangle outward by a uniform amount.
*	@param rect Source rectangle.
*	@param n Amount to expand on each side.
*	@return The expanded rectangle.
*/
static mu_Rect expand_rect(mu_Rect rect, int n) {
  return mu_rect(rect.x - n, rect.y - n, rect.w + n * 2, rect.h + n * 2);
}


/**
*	@brief	Intersect two rectangles in integer pixel space.
*	@param r1 First rectangle.
*	@param r2 Second rectangle.
*	@return The overlapped region, or an empty rectangle.
*/
static mu_Rect intersect_rects(mu_Rect r1, mu_Rect r2) {
  int x1 = mu_max(r1.x, r2.x);
  int y1 = mu_max(r1.y, r2.y);
  int x2 = mu_min(r1.x + r1.w, r2.x + r2.w);
  int y2 = mu_min(r1.y + r1.h, r2.y + r2.h);
  if (x2 < x1) { x2 = x1; }
  if (y2 < y1) { y2 = y1; }
  return mu_rect(x1, y1, x2 - x1, y2 - y1);
}


/**
*	@brief	Test whether a point lies inside a rectangle.
*	@param r Rectangle to test.
*	@param p Point to test.
*	@return Non-zero if the point is inside the rectangle.
*/
static int rect_overlaps_vec2(mu_Rect r, mu_Vec2 p) {
  return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

/**
*	@brief Draw a frame with the given color id. This is used for drawing all colored
*	@param ctx The UI context
*	@param rect The rectangle to draw the frame in
*	@param colorid The color id to use for the frame. This corresponds to an index in the style's color array.
**/
static void draw_frame(mu_Context *ctx, mu_Rect rect, int colorid) {

	mu_draw_rect(ctx, rect, ctx->style->colors[colorid]);
  if (colorid == MU_COLOR_SCROLLBASE  ||
      colorid == MU_COLOR_SCROLLTHUMB ||
      colorid == MU_COLOR_TITLEBG_INACTIVE ||
	  colorid == MU_COLOR_TITLEBG_ACTIVE ) { return; }
  /* draw border */
  if (ctx->style->colors[MU_COLOR_BORDER].a) {
    mu_draw_box(ctx, expand_rect(rect, 1), ctx->style->colors[MU_COLOR_BORDER]);
  }
}


/**
*	@brief	Initialize a context and bind the default draw callback and style.
*	@param ctx Context to initialize.
*/
void mu_init(mu_Context *ctx) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->draw_frame = draw_frame;
  ctx->_style = default_style;
  ctx->style = &ctx->_style;
}


/**
*	@brief	Begin a new UI frame and reset per-frame state.
*	@param ctx Active context.
*/
void mu_begin(mu_Context *ctx) {
  expect(ctx->text_width && ctx->text_height);
  ctx->command_list.idx = 0;
  ctx->root_list.idx = 0;
  ctx->scroll_target = NULL;
  ctx->hover_root = ctx->next_hover_root;
  ctx->next_hover_root = NULL;
  ctx->mouse_delta.x = ctx->mouse_pos.x - ctx->last_mouse_pos.x;
  ctx->mouse_delta.y = ctx->mouse_pos.y - ctx->last_mouse_pos.y;
  ctx->frame++;
}


/**
*	@brief	Compare containers by z-index for sorting.
*	@param a First container pointer wrapper.
*	@param b Second container pointer wrapper.
*	@return Negative, zero, or positive ordering result.
*/
static int compare_zindex(const void *a, const void *b) {
  return (*(mu_Container**) a)->zindex - (*(mu_Container**) b)->zindex;
}


/**
*	@brief	End a UI frame and finalize root container command linking.
*	@param ctx Active context.
*/
void mu_end(mu_Context *ctx) {
  int i, n;
  /* check stacks */
  expect(ctx->container_stack.idx == 0);
  expect(ctx->clip_stack.idx      == 0);
  expect(ctx->id_stack.idx        == 0);
  expect(ctx->layout_stack.idx    == 0);

  /* handle scroll input */
  if (ctx->scroll_target) {
    ctx->scroll_target->scroll.x += ctx->scroll_delta.x;
    ctx->scroll_target->scroll.y += ctx->scroll_delta.y;
  }

  /* unset focus if focus id was not touched this frame */
  if (!ctx->updated_focus) { ctx->focus = 0; }
  ctx->updated_focus = 0;

  /* bring hover root to front if mouse was pressed */
  if (ctx->mouse_pressed && ctx->next_hover_root &&
      ctx->next_hover_root->zindex < ctx->last_zindex &&
      ctx->next_hover_root->zindex >= 0
  ) {
    mu_bring_to_front(ctx, ctx->next_hover_root);
  }

  /* reset input state */
  ctx->key_pressed = 0;
  ctx->input_text[0] = '\0';
  ctx->mouse_pressed = 0;
  ctx->scroll_delta = mu_vec2(0, 0);
  ctx->last_mouse_pos = ctx->mouse_pos;

  /* sort root containers by zindex */
  n = ctx->root_list.idx;
  qsort(ctx->root_list.items, n, sizeof(mu_Container*), compare_zindex);

  /* set root container jump commands */
  for (i = 0; i < n; i++) {
    mu_Container *cnt = ctx->root_list.items[i];
    /* if this is the first container then make the first command jump to it.
    ** otherwise set the previous container's tail to jump to this one */
    if (i == 0) {
      mu_Command *cmd = (mu_Command*) ctx->command_list.items;
      cmd->jump.dst = (char*) cnt->head + sizeof(mu_JumpCommand);
    } else {
      mu_Container *prev = ctx->root_list.items[i - 1];
      prev->tail->jump.dst = (char*) cnt->head + sizeof(mu_JumpCommand);
    }
    /* make the last container's tail jump to the end of command list */
    if (i == n - 1) {
      cnt->tail->jump.dst = ctx->command_list.items + ctx->command_list.idx;
    }
  }
}


/**
*	@brief	Set the currently focused widget ID.
*	@param ctx Active context.
*	@param id Widget identifier to focus.
*/
void mu_set_focus(mu_Context *ctx, mu_Id id) {
  ctx->focus = id;
  ctx->updated_focus = 1;
}


/**
*	@brief	Seed used by the 32-bit FNV-1a hash implementation.
*/
#define HASH_INITIAL 2166136261

/**
*	@brief	Update a widget hash from arbitrary byte data.
*	@param hash Mutable hash accumulator.
*	@param data Input bytes.
*	@param size Input size in bytes.
*/
static void hash(mu_Id *hash, const void *data, int size) {
  const unsigned char *p = data;
  while (size--) {
    *hash = (*hash ^ *p++) * 16777619;
  }
}


/**
*	@brief	Hash arbitrary bytes with the current ID stack to produce a stable widget ID.
*	@param ctx Active context.
*	@param data Seed data used to derive the ID.
*	@param size Size of the seed data in bytes.
*	@return A stable widget ID.
*/
mu_Id mu_get_id(mu_Context *ctx, const void *data, int size) {
  int idx = ctx->id_stack.idx;
  mu_Id res = (idx > 0) ? ctx->id_stack.items[idx - 1] : HASH_INITIAL;
  hash(&res, data, size);
  ctx->last_id = res;
  return res;
}


/**
*	@brief	Push a scope ID seed for nested ID generation.
*	@param ctx Active context.
*	@param data Seed data.
*	@param size Size of the seed data in bytes.
*/
void mu_push_id(mu_Context *ctx, const void *data, int size) {
  push(ctx->id_stack, mu_get_id(ctx, data, size));
}


/**
*	@brief	Pop the most recent scope ID seed.
*	@param ctx Active context.
*/
void mu_pop_id(mu_Context *ctx) {
  pop(ctx->id_stack);
}


/**
*	@brief	Push and intersect a new clipping rectangle.
*	@param ctx Active context.
*	@param rect Rectangle to intersect with the current clip region.
*/
void mu_push_clip_rect(mu_Context *ctx, mu_Rect rect) {
  mu_Rect last = mu_get_clip_rect(ctx);
  push(ctx->clip_stack, intersect_rects(rect, last));
}


/**
*	@brief	Restore the previous clipping rectangle.
*	@param ctx Active context.
*/
void mu_pop_clip_rect(mu_Context *ctx) {
  pop(ctx->clip_stack);
}


/**
*	@brief	Get the current effective clipping rectangle.
*	@param ctx Active context.
*	@return The active clip rectangle.
*/
mu_Rect mu_get_clip_rect(mu_Context *ctx) {
  expect(ctx->clip_stack.idx > 0);
  return ctx->clip_stack.items[ctx->clip_stack.idx - 1];
}


/**
*	@brief	Test a rectangle against the current clip region.
*	@param ctx Active context.
*	@param r Rectangle to test.
*	@return A clip mode value.
*/
int mu_check_clip(mu_Context *ctx, mu_Rect r) {
  mu_Rect cr = mu_get_clip_rect(ctx);
  if (r.x > cr.x + cr.w || r.x + r.w < cr.x ||
      r.y > cr.y + cr.h || r.y + r.h < cr.y   ) { return MU_CLIP_ALL; }
  if (r.x >= cr.x && r.x + r.w <= cr.x + cr.w &&
      r.y >= cr.y && r.y + r.h <= cr.y + cr.h ) { return 0; }
  return MU_CLIP_PART;
}


static void push_layout(mu_Context *ctx, mu_Rect body, mu_Vec2 scroll) {
  mu_Layout layout;
  int width = 0;
  memset(&layout, 0, sizeof(layout));
  layout.body = mu_rect(body.x - scroll.x, body.y - scroll.y, body.w, body.h);
  layout.max = mu_vec2(-0x1000000, -0x1000000);
  push(ctx->layout_stack, layout);
  mu_layout_row(ctx, 1, &width, 0);
}


static mu_Layout* get_layout(mu_Context *ctx) {
  return &ctx->layout_stack.items[ctx->layout_stack.idx - 1];
}


static void pop_container(mu_Context *ctx) {
  mu_Container *cnt = mu_get_current_container(ctx);
  mu_Layout *layout = get_layout(ctx);
  cnt->content_size.x = layout->max.x - layout->body.x;
  cnt->content_size.y = layout->max.y - layout->body.y;
  /* pop container, layout and id */
  pop(ctx->container_stack);
  pop(ctx->layout_stack);
  mu_pop_id(ctx);
}


mu_Container* mu_get_current_container(mu_Context *ctx) {
  expect(ctx->container_stack.idx > 0);
  return ctx->container_stack.items[ ctx->container_stack.idx - 1 ];
}


static mu_Container* get_container(mu_Context *ctx, mu_Id id, int opt) {
  mu_Container *cnt;
  /* try to get existing container from pool */
  int idx = mu_pool_get(ctx, ctx->container_pool, MU_CONTAINERPOOL_SIZE, id);
  if (idx >= 0) {
    if (ctx->containers[idx].open || ~opt & MU_OPT_CLOSED) {
      mu_pool_update(ctx, ctx->container_pool, idx);
    }
    return &ctx->containers[idx];
  }
  if (opt & MU_OPT_CLOSED) { return NULL; }
  /* container not found in pool: init new container */
  idx = mu_pool_init(ctx, ctx->container_pool, MU_CONTAINERPOOL_SIZE, id);
  cnt = &ctx->containers[idx];
  memset(cnt, 0, sizeof(*cnt));
  cnt->open = 1;
  mu_bring_to_front(ctx, cnt);
  return cnt;
}


mu_Container* mu_get_container(mu_Context *ctx, const char *name) {
  mu_Id id = mu_get_id(ctx, name, strlen(name));
  return get_container(ctx, id, 0);
}


void mu_bring_to_front(mu_Context *ctx, mu_Container *cnt) {
  cnt->zindex = ++ctx->last_zindex;
}


/*============================================================================
** pool
**============================================================================*/

int mu_pool_init(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i, n = -1, f = ctx->frame;
  for (i = 0; i < len; i++) {
    if (items[i].last_update < f) {
      f = items[i].last_update;
      n = i;
    }
  }
  expect(n > -1);
  items[n].id = id;
  mu_pool_update(ctx, items, n);
  return n;
}


/**
*	@brief\tLook up a retained pool item by ID.
*	@param ctx Active context.
*	@param items Pool entries.
*	@param len Pool length.
*	@param id Identifier to search for.
*	@return Index of the matching slot or `-1`.
*/
int mu_pool_get(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i;
  unused(ctx);
  for (i = 0; i < len; i++) {
    if (items[i].id == id) { return i; }
  }
  return -1;
}


/**
*	@brief\tRefresh the update time for a pool entry.
*	@param ctx Active context.
*	@param items Pool entries.
*	@param idx Slot index to refresh.
*/
void mu_pool_update(mu_Context *ctx, mu_PoolItem *items, int idx) {
  items[idx].last_update = ctx->frame;
}


/**
*	@brief\tInput handler entry points.
*	@note\tThese functions translate platform input into frame-local UI state.
*/
/*============================================================================
** input handlers
**============================================================================*/

/**
*	@brief\tUpdate the current mouse position.
*	@param ctx Active context.
*	@param x Mouse x coordinate.
*	@param y Mouse y coordinate.
*/
void mu_input_mousemove(mu_Context *ctx, int x, int y) {
  ctx->mouse_pos = mu_vec2(x, y);
}


/**
*	@brief\tRegister a mouse-button press.
*	@param ctx Active context.
*	@param x Mouse x coordinate.
*	@param y Mouse y coordinate.
*	@param btn Mouse button bitmask.
*/
void mu_input_mousedown(mu_Context *ctx, int x, int y, int btn) {
  mu_input_mousemove(ctx, x, y);
  ctx->mouse_down |= btn;
  ctx->mouse_pressed |= btn;
}


/**
*	@brief\tRegister a mouse-button release.
*	@param ctx Active context.
*	@param x Mouse x coordinate.
*	@param y Mouse y coordinate.
*	@param btn Mouse button bitmask.
*/
void mu_input_mouseup(mu_Context *ctx, int x, int y, int btn) {
  mu_input_mousemove(ctx, x, y);
  ctx->mouse_down &= ~btn;
}


/**
*	@brief\tAccumulate mouse-wheel scrolling.
*	@param ctx Active context.
*	@param x Horizontal scroll delta.
*	@param y Vertical scroll delta.
*/
void mu_input_scroll(mu_Context *ctx, int x, int y) {
  ctx->scroll_delta.x += x;
  ctx->scroll_delta.y += y;
}


/**
*	@brief\tRegister a key press.
*	@param ctx Active context.
*	@param key Key bitmask.
*/
void mu_input_keydown(mu_Context *ctx, int key) {
  ctx->key_pressed |= key;
  ctx->key_down |= key;
}


/**
*	@brief\tRegister a key release.
*	@param ctx Active context.
*	@param key Key bitmask.
*/
void mu_input_keyup(mu_Context *ctx, int key) {
  ctx->key_down &= ~key;
}


/**
*	@brief\tAppend UTF-8 input text to the frame buffer.
*	@param ctx Active context.
*	@param text UTF-8 text to append.
*/
void mu_input_text(mu_Context *ctx, const char *text) {
  int len = strlen(ctx->input_text);
  int size = strlen(text) + 1;
  expect(len + size <= (int) sizeof(ctx->input_text));
  memcpy(ctx->input_text + len, text, size);
}


/**
*	@brief\tCommand stream helpers.
*	@note\tThese functions append and iterate packed draw/control commands.
*/
/*============================================================================
** commandlist
**============================================================================*/

/**
*	@brief\tPush a command payload onto the current command list.
*	@param ctx Active context.
*	@param type Command type tag.
*	@param size Command size in bytes.
*	@return Pointer to the new command.
*/
mu_Command* mu_push_command(mu_Context *ctx, int type, int size) {
  mu_Command *cmd = (mu_Command*) (ctx->command_list.items + ctx->command_list.idx);
  expect(ctx->command_list.idx + size < MU_COMMANDLIST_SIZE);
  cmd->base.type = type;
  cmd->base.size = size;
  ctx->command_list.idx += size;
  return cmd;
}


/**
*	@brief\tAdvance to the next executable command in the command list.
*	@param ctx Active context.
*	@param cmd Current iterator pointer.
*	@return Non-zero if a command was produced.
*/
int mu_next_command(mu_Context *ctx, mu_Command **cmd) {
  if (*cmd) {
    *cmd = (mu_Command*) (((char*) *cmd) + (*cmd)->base.size);
  } else {
    *cmd = (mu_Command*) ctx->command_list.items;
  }
  while ((char*) *cmd != ctx->command_list.items + ctx->command_list.idx) {
    if ((*cmd)->type != MU_COMMAND_JUMP) { return 1; }
    *cmd = (*cmd)->jump.dst;
  }
  return 0;
}


/**
*	@brief\tPush a jump command to a destination command.
*	@param ctx Active context.
*	@param dst Destination command pointer.
*	@return The emitted jump command.
*/
static mu_Command* push_jump(mu_Context *ctx, mu_Command *dst) {
  mu_Command *cmd;
  cmd = mu_push_command(ctx, MU_COMMAND_JUMP, sizeof(mu_JumpCommand));
  cmd->jump.dst = dst;
  return cmd;
}


/**
*	@brief\tEmit a clip command.
*	@param ctx Active context.
*	@param rect Clip rectangle.
*/
void mu_set_clip(mu_Context *ctx, mu_Rect rect) {
  mu_Command *cmd;
  cmd = mu_push_command(ctx, MU_COMMAND_CLIP, sizeof(mu_ClipCommand));
  cmd->clip.rect = rect;
}


/**
*	@brief\tEmit a filled rectangle draw command.
*	@param ctx Active context.
*	@param rect Rectangle to draw.
*	@param color Fill color.
*/
void mu_draw_rect(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_Command *cmd;
  rect = intersect_rects(rect, mu_get_clip_rect(ctx));
  if (rect.w > 0 && rect.h > 0) {
    cmd = mu_push_command(ctx, MU_COMMAND_RECT, sizeof(mu_RectCommand));
    cmd->rect.rect = rect;
    cmd->rect.color = color;
  }
}


/**
*	@brief\tEmit a 1-pixel box outline using four rectangles.
*	@param ctx Active context.
*	@param rect Box rectangle.
*	@param color Outline color.
*/
void mu_draw_box(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_draw_rect(ctx, mu_rect(rect.x + 1, rect.y, rect.w - 2, 1), color);
  mu_draw_rect(ctx, mu_rect(rect.x + 1, rect.y + rect.h - 1, rect.w - 2, 1), color);
  mu_draw_rect(ctx, mu_rect(rect.x, rect.y, 1, rect.h), color);
  mu_draw_rect(ctx, mu_rect(rect.x + rect.w - 1, rect.y, 1, rect.h), color);
}


/**
*	@brief\tEmit a text draw command.
*	@param ctx Active context.
*	@param font Font handle.
*	@param str UTF-8 string.
*	@param len String length or `-1` for null-terminated input.
*	@param pos Text position.
*	@param color Text color.
*/
void mu_draw_text(mu_Context *ctx, mu_Font font, const char *str, int len,
  mu_Vec2 pos, mu_Color color)
{
  mu_Command *cmd;
  mu_Rect rect = mu_rect(
    pos.x, pos.y, ctx->text_width(font, str, len), ctx->text_height(font));
  int clipped = mu_check_clip(ctx, rect);
  if (clipped == MU_CLIP_ALL ) { return; }
  if (clipped == MU_CLIP_PART) { mu_set_clip(ctx, mu_get_clip_rect(ctx)); }
  /* add command */
  if (len < 0) { len = strlen(str); }
  cmd = mu_push_command(ctx, MU_COMMAND_TEXT, sizeof(mu_TextCommand) + len);
  memcpy(cmd->text.str, str, len);
  cmd->text.str[len] = '\0';
  cmd->text.pos = pos;
  cmd->text.color = color;
  cmd->text.font = font;
  /* reset clipping if it was set */
  if (clipped) { mu_set_clip(ctx, unclipped_rect); }
}


/**
*	@brief\tEmit an icon draw command.
*	@param ctx Active context.
*	@param id Icon identifier.
*	@param rect Destination rectangle.
*	@param color Tint color.
*/
void mu_draw_icon(mu_Context *ctx, int id, mu_Rect rect, mu_Color color) {
  mu_Command *cmd;
  /* do clip command if the rect isn't fully contained within the cliprect */
  int clipped = mu_check_clip(ctx, rect);
  if (clipped == MU_CLIP_ALL ) { return; }
  if (clipped == MU_CLIP_PART) { mu_set_clip(ctx, mu_get_clip_rect(ctx)); }
  /* do icon command */
  cmd = mu_push_command(ctx, MU_COMMAND_ICON, sizeof(mu_IconCommand));
  cmd->icon.id = id;
  cmd->icon.rect = rect;
  cmd->icon.color = color;
  /* reset clipping if it was set */
  if (clipped) { mu_set_clip(ctx, unclipped_rect); }
}


/**
*	@brief\tLayout state helpers.
*	@note\tThese functions manage row/column flow and deferred placement.
*/
/*============================================================================
** layout
**============================================================================*/

enum { RELATIVE = 1, ABSOLUTE = 2 };


/**
*	@brief\tBegin a nested column layout.
*	@param ctx Active context.
*/
void mu_layout_begin_column(mu_Context *ctx) {
  push_layout(ctx, mu_layout_next(ctx), mu_vec2(0, 0));
}


/**
*	@brief\tEnd the current nested column layout.
*	@param ctx Active context.
*/
void mu_layout_end_column(mu_Context *ctx) {
  mu_Layout *a, *b;
  b = get_layout(ctx);
  pop(ctx->layout_stack);
  /* inherit position/next_row/max from child layout if they are greater */
  a = get_layout(ctx);
  a->position.x = mu_max(a->position.x, b->position.x + b->body.x - a->body.x);
  a->next_row = mu_max(a->next_row, b->next_row + b->body.y - a->body.y);
  a->max.x = mu_max(a->max.x, b->max.x);
  a->max.y = mu_max(a->max.y, b->max.y);
}


/**
*	@brief\tConfigure the current layout row.
*	@param ctx Active context.
*	@param items Number of items in the row.
*	@param widths Per-item widths, or `NULL` for automatic sizing.
*	@param height Row height.
*/
void mu_layout_row(mu_Context *ctx, int items, const int *widths, int height) {
  mu_Layout *layout = get_layout(ctx);
  if (widths) {
    expect(items <= MU_MAX_WIDTHS);
    memcpy(layout->widths, widths, items * sizeof(widths[0]));
  }
  layout->items = items;
  layout->position = mu_vec2(layout->indent, layout->next_row);
  layout->size.y = height;
  layout->item_index = 0;
}


/**
*	@brief\tOverride the next item width.
*	@param ctx Active context.
*	@param width Desired width.
*/
void mu_layout_width(mu_Context *ctx, int width) {
  get_layout(ctx)->size.x = width;
}


/**
*	@brief\tOverride the next item height.
*	@param ctx Active context.
*	@param height Desired height.
*/
void mu_layout_height(mu_Context *ctx, int height) {
  get_layout(ctx)->size.y = height;
}


/**
*	@brief\tSet the next layout rectangle.
*	@param ctx Active context.
*	@param r Rectangle to use for the next item.
*	@param relative Non-zero to treat the rectangle as relative to the body.
*/
void mu_layout_set_next(mu_Context *ctx, mu_Rect r, int relative) {
  mu_Layout *layout = get_layout(ctx);
  layout->next = r;
  layout->next_type = relative ? RELATIVE : ABSOLUTE;
}


/**
*	@brief\tAdvance the layout and return the next item rectangle.
*	@param ctx Active context.
*	@return The next layout rectangle.
*/
mu_Rect mu_layout_next(mu_Context *ctx) {
  mu_Layout *layout = get_layout(ctx);
  mu_Style *style = ctx->style;
  mu_Rect res;

  if (layout->next_type) {
    /* handle rect set by `mu_layout_set_next` */
    int type = layout->next_type;
    layout->next_type = 0;
    res = layout->next;
    if (type == ABSOLUTE) { return (ctx->last_rect = res); }

  } else {
    /* handle next row */
    if (layout->item_index == layout->items) {
      mu_layout_row(ctx, layout->items, NULL, layout->size.y);
    }

    /* position */
    res.x = layout->position.x;
    res.y = layout->position.y;

    /* size */
    res.w = layout->items > 0 ? layout->widths[layout->item_index] : layout->size.x;
    res.h = layout->size.y;
    if (res.w == 0) { res.w = style->size.x + style->padding * 2; }
    if (res.h == 0) { res.h = style->size.y + style->padding * 2; }
    if (res.w <  0) { res.w += layout->body.w - res.x + 1; }
    if (res.h <  0) { res.h += layout->body.h - res.y + 1; }

    layout->item_index++;
  }

  /* update position */
  layout->position.x += res.w + style->spacing;
  layout->next_row = mu_max(layout->next_row, res.y + res.h + style->spacing);

  /* apply body offset */
  res.x += layout->body.x;
  res.y += layout->body.y;

  /* update max position */
  layout->max.x = mu_max(layout->max.x, res.x + res.w);
  layout->max.y = mu_max(layout->max.y, res.y + res.h);

  return (ctx->last_rect = res);
}


/**
*	@brief\tInteractive control helpers.
*	@note\tThese functions update hover/focus state and draw common widget chrome.
*/
/*============================================================================
** controls
**============================================================================*/

/**
*	@brief\tTest whether the active hover root matches the current container stack.
*	@param ctx Active context.
*	@return Non-zero when the mouse is still inside the hover root chain.
*/
static int in_hover_root(mu_Context *ctx) {
  int i = ctx->container_stack.idx;
  while (i--) {
    if (ctx->container_stack.items[i] == ctx->hover_root) { return 1; }
    /* only root containers have their `head` field set; stop searching if we've
    ** reached the current root container */
    if (ctx->container_stack.items[i]->head) { break; }
  }
  return 0;
}


/**
*	@brief\tDraw a control frame using the state-dependent theme color.
*	@param ctx Active context.
*	@param id Widget identifier.
*	@param rect Control rectangle.
*	@param colorid Base color index.
*	@param opt Control option bitmask.
*/
void mu_draw_control_frame(mu_Context *ctx, mu_Id id, mu_Rect rect,
  int colorid, int opt)
{
  if (opt & MU_OPT_NOFRAME) { return; }
  colorid += (ctx->focus == id) ? 2 : (ctx->hover == id) ? 1 : 0;
  ctx->draw_frame(ctx, rect, colorid);
}


/**
*	@brief\tDraw control text clipped to a rectangle.
*	@param ctx Active context.
*	@param str Text string.
*	@param rect Control rectangle.
*	@param colorid Theme color index.
*	@param opt Control option bitmask.
*/
void mu_draw_control_text(mu_Context *ctx, const char *str, mu_Rect rect,
  int colorid, int opt)
{
  mu_Vec2 pos;
  mu_Font font = ctx->style->font;
  int tw = ctx->text_width(font, str, -1);
  mu_push_clip_rect(ctx, rect);
  pos.y = rect.y + (rect.h - ctx->text_height(font)) / 2;
  if (opt & MU_OPT_ALIGNCENTER) {
    pos.x = rect.x + (rect.w - tw) / 2;
  } else if (opt & MU_OPT_ALIGNRIGHT) {
    pos.x = rect.x + rect.w - tw - ctx->style->padding;
  } else {
    pos.x = rect.x + ctx->style->padding;
  }
  mu_draw_text(ctx, font, str, -1, pos, ctx->style->colors[colorid]);
  mu_pop_clip_rect(ctx);
}


/**
*	@brief\tTest whether the mouse is over a rectangle and the active hover root.
*	@param ctx Active context.
*	@param rect Rectangle to test.
*	@return Non-zero when the rectangle is under the cursor.
*/
int mu_mouse_over(mu_Context *ctx, mu_Rect rect) {
  return rect_overlaps_vec2(rect, ctx->mouse_pos) &&
    rect_overlaps_vec2(mu_get_clip_rect(ctx), ctx->mouse_pos) &&
    in_hover_root(ctx);
}


/**
*	@brief\tUpdate hover and focus state for a control.
*	@param ctx Active context.
*	@param id Widget identifier.
*	@param rect Control rectangle.
*	@param opt Control option bitmask.
*/
void mu_update_control(mu_Context *ctx, mu_Id id, mu_Rect rect, int opt) {
  int mouseover = mu_mouse_over(ctx, rect);

  if (ctx->focus == id) { ctx->updated_focus = 1; }
  if (opt & MU_OPT_NOINTERACT) { return; }
  if (mouseover && !ctx->mouse_down) { ctx->hover = id; }

  if (ctx->focus == id) {
    if (ctx->mouse_pressed && !mouseover) { mu_set_focus(ctx, 0); }
    if (!ctx->mouse_down && ~opt & MU_OPT_HOLDFOCUS) { mu_set_focus(ctx, 0); }
  }

  if (ctx->hover == id) {
    if (ctx->mouse_pressed) {
      mu_set_focus(ctx, id);
    } else if (!mouseover) {
      ctx->hover = 0;
    }
  }
}


/**
*	@brief\tEmit static text using the current layout.
*	@param ctx Active context.
*	@param text UTF-8 text to draw.
*/
void mu_text(mu_Context *ctx, const char *text) {
  const char *start, *end, *p = text;
  int width = -1;
  mu_Font font = ctx->style->font;
  mu_Color color = ctx->style->colors[MU_COLOR_TEXT];
  mu_layout_begin_column(ctx);
  mu_layout_row(ctx, 1, &width, ctx->text_height(font));
  do {
    mu_Rect r = mu_layout_next(ctx);
    int w = 0;
    start = end = p;
    do {
      const char* word = p;
      while (*p && *p != ' ' && *p != '\n') { p++; }
      w += ctx->text_width(font, word, p - word);
      if (w > r.w && end != start) { break; }
      w += ctx->text_width(font, p, 1);
      end = p++;
    } while (*end && *end != '\n');
    mu_draw_text(ctx, font, start, end - start, mu_vec2(r.x, r.y), color);
    p = end + 1;
  } while (*end);
  mu_layout_end_column(ctx);
}


/**
*	@brief\tEmit a single-line label using the current layout.
*	@param ctx Active context.
*	@param text Label text.
*/
void mu_label(mu_Context *ctx, const char *text) {
  mu_draw_control_text(ctx, text, mu_layout_next(ctx), MU_COLOR_TEXT, 0);
}


/**
*	@brief\tEmit a button widget.
*	@param ctx Active context.
*	@param label Button label, or `NULL` when using an icon.
*	@param icon Icon identifier.
*	@param opt Control option bitmask.
*	@return Widget result flags.
*/
int mu_button_ex(mu_Context *ctx, const char *label, int icon, int opt) {
  int res = 0;
  mu_Id id = label ? mu_get_id(ctx, label, strlen(label))
                   : mu_get_id(ctx, &icon, sizeof(icon));
  mu_Rect r = mu_layout_next(ctx);
  mu_update_control(ctx, id, r, opt);
  /* handle click */
  if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
    res |= MU_RES_SUBMIT;
  }
  /* draw */
  mu_draw_control_frame(ctx, id, r, MU_COLOR_BUTTON, opt);
  if (label) { mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, opt); }
  if (icon) { mu_draw_icon(ctx, icon, r, ctx->style->colors[MU_COLOR_TEXT]); }
  return res;
}


/**
*	@brief\tEmit a checkbox widget.
*	@param ctx Active context.
*	@param label Checkbox label.
*	@param state Checkbox state pointer.
*	@return Widget result flags.
*/
int mu_checkbox(mu_Context *ctx, const char *label, int *state) {
  int res = 0;
  mu_Id id = mu_get_id(ctx, &state, sizeof(state));
  mu_Rect r = mu_layout_next(ctx);
  mu_Rect box = mu_rect(r.x, r.y, r.h, r.h);
  mu_update_control(ctx, id, r, 0);
  /* handle click */
  if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
    res |= MU_RES_CHANGE;
    *state = !*state;
  }
  /* draw */
  mu_draw_control_frame(ctx, id, box, MU_COLOR_BASE, 0);
  if (*state) {
    mu_draw_icon(ctx, MU_ICON_CHECK, box, ctx->style->colors[MU_COLOR_TEXT]);
  }
  r = mu_rect(r.x + box.w, r.y, r.w - box.w, r.h);
  mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, 0);
  return res;
}


/**
*	@brief\tEdit a raw text buffer in-place.
*	@param ctx Active context.
*	@param buf Editable buffer.
*	@param bufsz Buffer size.
*	@param id Widget identifier.
*	@param r Control rectangle.
*	@param opt Control option bitmask.
*	@return Widget result flags.
*/
int mu_textbox_raw(mu_Context *ctx, char *buf, int bufsz, mu_Id id, mu_Rect r,
  int opt)
{
  int res = 0;
  mu_update_control(ctx, id, r, opt | MU_OPT_HOLDFOCUS);

  if (ctx->focus == id) {
    /* handle text input */
    int len = strlen(buf);
    int n = mu_min(bufsz - len - 1, (int) strlen(ctx->input_text));
    if (n > 0) {
      memcpy(buf + len, ctx->input_text, n);
      len += n;
      buf[len] = '\0';
      res |= MU_RES_CHANGE;
    }
    /* handle backspace */
    if (ctx->key_pressed & MU_KEY_BACKSPACE && len > 0) {
      /* skip utf-8 continuation bytes */
      while ((buf[--len] & 0xc0) == 0x80 && len > 0);
      buf[len] = '\0';
      res |= MU_RES_CHANGE;
    }
    /* handle return */
    if (ctx->key_pressed & MU_KEY_RETURN) {
      mu_set_focus(ctx, 0);
      res |= MU_RES_SUBMIT;
    }
  }

  /* draw */
  mu_draw_control_frame(ctx, id, r, MU_COLOR_BASE, opt);
  if (ctx->focus == id) {
    mu_Color color = ctx->style->colors[MU_COLOR_TEXT];
    mu_Font font = ctx->style->font;
    int textw = ctx->text_width(font, buf, -1);
    int texth = ctx->text_height(font);
    int ofx = r.w - ctx->style->padding - textw - 1;
    int textx = r.x + mu_min(ofx, ctx->style->padding);
    int texty = r.y + (r.h - texth) / 2;
    mu_push_clip_rect(ctx, r);
    mu_draw_text(ctx, font, buf, -1, mu_vec2(textx, texty), color);
    mu_draw_rect(ctx, mu_rect(textx + textw, texty, 1, texth), color);
    mu_pop_clip_rect(ctx);
  } else {
    mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, opt);
  }

  return res;
}


/**
*	@brief\tHandle numeric text editing mode for sliders and number boxes.
*	@param ctx Active context.
*	@param value Numeric value pointer.
*	@param r Control rectangle.
*	@param id Widget identifier.
*	@return Non-zero while text-edit mode is still active.
*/
static int number_textbox(mu_Context *ctx, mu_Real *value, mu_Rect r, mu_Id id) {
  if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->key_down & MU_KEY_SHIFT &&
      ctx->hover == id
  ) {
    ctx->number_edit = id;
    sprintf(ctx->number_edit_buf, MU_REAL_FMT, *value);
  }
  if (ctx->number_edit == id) {
    int res = mu_textbox_raw(
      ctx, ctx->number_edit_buf, sizeof(ctx->number_edit_buf), id, r, 0);
    if (res & MU_RES_SUBMIT || ctx->focus != id) {
      *value = strtod(ctx->number_edit_buf, NULL);
      ctx->number_edit = 0;
    } else {
      return 1;
    }
  }
  return 0;
}


/**
*	@brief\tEmit a standard text box widget.
*	@param ctx Active context.
*	@param buf Editable buffer.
*	@param bufsz Buffer size.
*	@param opt Control option bitmask.
*	@return Widget result flags.
*/
int mu_textbox_ex(mu_Context *ctx, char *buf, int bufsz, int opt) {
  mu_Id id = mu_get_id(ctx, &buf, sizeof(buf));
  mu_Rect r = mu_layout_next(ctx);
  return mu_textbox_raw(ctx, buf, bufsz, id, r, opt);
}


/**
*	@brief\tEmit a slider widget.
*	@param ctx Active context.
*	@param value Slider value pointer.
*	@param low Minimum value.
*	@param high Maximum value.
*	@param step Quantization step.
*	@param fmt Display format.
*	@param opt Control option bitmask.
*	@return Widget result flags.
*/
int mu_slider_ex(mu_Context *ctx, mu_Real *value, mu_Real low, mu_Real high,
  mu_Real step, const char *fmt, int opt)
{
  char buf[MU_MAX_FMT + 1];
  mu_Rect thumb;
  int x, w, res = 0;
  mu_Real last = *value, v = last;
  mu_Id id = mu_get_id(ctx, &value, sizeof(value));
  mu_Rect base = mu_layout_next(ctx);

  /* handle text input mode */
  if (number_textbox(ctx, &v, base, id)) { return res; }

  /* handle normal mode */
  mu_update_control(ctx, id, base, opt);

  /* handle input */
  if (ctx->focus == id &&
      (ctx->mouse_down | ctx->mouse_pressed) == MU_MOUSE_LEFT)
  {
    v = low + (ctx->mouse_pos.x - base.x) * (high - low) / base.w;
    if (step) { v = ((long long)((v + step / 2) / step)) * step; }
  }
  /* clamp and store value, update res */
  *value = v = mu_clamp(v, low, high);
  if (last != v) { res |= MU_RES_CHANGE; }

  /* draw base */
  mu_draw_control_frame(ctx, id, base, MU_COLOR_BASE, opt);
  /* draw thumb */
  w = ctx->style->thumb_size;
  x = (v - low) * (base.w - w) / (high - low);
  thumb = mu_rect(base.x + x, base.y, w, base.h);
  mu_draw_control_frame(ctx, id, thumb, MU_COLOR_BUTTON, opt);
  /* draw text  */
  sprintf(buf, fmt, v);
  mu_draw_control_text(ctx, buf, base, MU_COLOR_TEXT, opt);

  return res;
}


/**
*	@brief\tEmit a numeric entry widget.
*	@param ctx Active context.
*	@param value Numeric value pointer.
*	@param step Dragging step size.
*	@param fmt Display format.
*	@param opt Control option bitmask.
*	@return Widget result flags.
*/
int mu_number_ex(mu_Context *ctx, mu_Real *value, mu_Real step,
  const char *fmt, int opt)
{
  char buf[MU_MAX_FMT + 1];
  int res = 0;
  mu_Id id = mu_get_id(ctx, &value, sizeof(value));
  mu_Rect base = mu_layout_next(ctx);
  mu_Real last = *value;

  /* handle text input mode */
  if (number_textbox(ctx, value, base, id)) { return res; }

  /* handle normal mode */
  mu_update_control(ctx, id, base, opt);

  /* handle input */
  if (ctx->focus == id && ctx->mouse_down == MU_MOUSE_LEFT) {
    *value += ctx->mouse_delta.x * step;
  }
  /* set flag if value changed */
  if (*value != last) { res |= MU_RES_CHANGE; }

  /* draw base */
  mu_draw_control_frame(ctx, id, base, MU_COLOR_BASE, opt);
  /* draw text  */
  sprintf(buf, fmt, *value);
  mu_draw_control_text(ctx, buf, base, MU_COLOR_TEXT, opt);

  return res;
}


/**
*	@brief\tBuild shared header/tree-node behavior.
*	@param ctx Active context.
*	@param label Header label.
*	@param istreenode Non-zero for treenode styling.
*	@param opt Control option bitmask.
*	@return Widget result flags.
*/
static int header(mu_Context *ctx, const char *label, int istreenode, int opt) {
  mu_Rect r;
  int active, expanded;
  mu_Id id = mu_get_id(ctx, label, strlen(label));
  int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
  int width = -1;
  mu_layout_row(ctx, 1, &width, 0);

  active = (idx >= 0);
  expanded = (opt & MU_OPT_EXPANDED) ? !active : active;
  r = mu_layout_next(ctx);
  mu_update_control(ctx, id, r, 0);

  /* handle click */
  active ^= (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id);

  /* update pool ref */
  if (idx >= 0) {
    if (active) { mu_pool_update(ctx, ctx->treenode_pool, idx); }
           else { memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem)); }
  } else if (active) {
    mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
  }

  /* draw */
  if (istreenode) {
    if (ctx->hover == id) { ctx->draw_frame(ctx, r, MU_COLOR_BUTTONHOVER); }
  } else {
    mu_draw_control_frame(ctx, id, r, MU_COLOR_BUTTON, 0);
  }
  mu_draw_icon(
    ctx, expanded ? MU_ICON_EXPANDED : MU_ICON_COLLAPSED,
    mu_rect(r.x, r.y, r.h, r.h), ctx->style->colors[MU_COLOR_TEXT]);
  r.x += r.h - ctx->style->padding;
  r.w -= r.h - ctx->style->padding;
  mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, 0);

  return expanded ? MU_RES_ACTIVE : 0;
}


/**
*	@brief\tEmit a header widget.
*	@param ctx Active context.
*	@param label Header label.
*	@param opt Control option bitmask.
*	@return Widget result flags.
*/
int mu_header_ex(mu_Context *ctx, const char *label, int opt) {
  return header(ctx, label, 0, opt);
}


/**
*	@brief\tEmit a tree-node header widget.
*	@param ctx Active context.
*	@param label Node label.
*	@param opt Control option bitmask.
*	@return Widget result flags.
*/
int mu_begin_treenode_ex(mu_Context *ctx, const char *label, int opt) {
  int res = header(ctx, label, 1, opt);
  if (res & MU_RES_ACTIVE) {
    get_layout(ctx)->indent += ctx->style->indent;
    push(ctx->id_stack, ctx->last_id);
  }
  return res;
}


/**
*	@brief\tEnd the current tree node scope.
*	@param ctx Active context.
*/
void mu_end_treenode(mu_Context *ctx) {
  get_layout(ctx)->indent -= ctx->style->indent;
  mu_pop_id(ctx);
}


/**
*	@brief\tScrollbar helper for a single axis.
*	@param ctx Active context.
*	@param cnt Container being scrolled.
*	@param b Body rectangle pointer.
*	@param cs Content size vector.
*	@param x Axis selector macro token.
*	@param y Axis selector macro token.
*	@param w Width selector macro token.
*	@param h Height selector macro token.
*/
#define scrollbar(ctx, cnt, b, cs, x, y, w, h)                              \
  do {                                                                      \
    /* only add scrollbar if content size is larger than body */            \
    int maxscroll = cs.y - b->h;                                            \
                                                                            \
    if (maxscroll > 0 && b->h > 0) {                                        \
      mu_Rect base, thumb;                                                  \
      mu_Id id = mu_get_id(ctx, "!scrollbar" #y, 11);                       \
                                                                            \
      /* get sizing / positioning */                                        \
      base = *b;                                                            \
      base.x = b->x + b->w;                                                 \
      base.w = ctx->style->scrollbar_size;                                  \
                                                                            \
      /* handle input */                                                    \
      mu_update_control(ctx, id, base, 0);                                  \
      if (ctx->focus == id && ctx->mouse_down == MU_MOUSE_LEFT) {           \
        cnt->scroll.y += ctx->mouse_delta.y * cs.y / base.h;                \
      }                                                                     \
      /* clamp scroll to limits */                                          \
      cnt->scroll.y = mu_clamp(cnt->scroll.y, 0, maxscroll);                \
                                                                            \
      /* draw base and thumb */                                             \
      ctx->draw_frame(ctx, base, MU_COLOR_SCROLLBASE);                      \
      thumb = base;                                                         \
      thumb.h = mu_max(ctx->style->thumb_size, base.h * b->h / cs.y);       \
      thumb.y += cnt->scroll.y * (base.h - thumb.h) / maxscroll;            \
      ctx->draw_frame(ctx, thumb, MU_COLOR_SCROLLTHUMB);                    \
                                                                            \
      /* set this as the scroll_target (will get scrolled on mousewheel) */ \
      /* if the mouse is over it */                                         \
      if (mu_mouse_over(ctx, *b)) { ctx->scroll_target = cnt; }             \
    } else {                                                                \
      cnt->scroll.y = 0;                                                    \
    }                                                                       \
  } while (0)


static void scrollbars(mu_Context *ctx, mu_Container *cnt, mu_Rect *body) {
  int sz = ctx->style->scrollbar_size;
  mu_Vec2 cs = cnt->content_size;
  cs.x += ctx->style->padding * 2;
  cs.y += ctx->style->padding * 2;
  mu_push_clip_rect(ctx, *body);
  /* resize body to make room for scrollbars */
  if (cs.y > cnt->body.h) { body->w -= sz; }
  if (cs.x > cnt->body.w) { body->h -= sz; }
  /* to create a horizontal or vertical scrollbar almost-identical code is
  ** used; only the references to `x|y` `w|h` need to be switched */
  scrollbar(ctx, cnt, body, cs, x, y, w, h);
  scrollbar(ctx, cnt, body, cs, y, x, h, w);
  mu_pop_clip_rect(ctx);
}


/**
*	@brief\tPush container body layout and optional scrollbars.
*	@param ctx Active context.
*	@param cnt Container being configured.
*	@param body Content body rectangle.
*	@param opt Container option bitmask.
*/
static void push_container_body(
  mu_Context *ctx, mu_Container *cnt, mu_Rect body, int opt
) {
  if (~opt & MU_OPT_NOSCROLL) { scrollbars(ctx, cnt, &body); }
  push_layout(ctx, expand_rect(body, -ctx->style->padding), cnt->scroll);
  cnt->body = body;
}


/**
*	@brief\tBegin a root container and emit its linking jump command.
*	@param ctx Active context.
*	@param cnt Container to begin.
*/
static void begin_root_container(mu_Context *ctx, mu_Container *cnt) {
  push(ctx->container_stack, cnt);
  /* push container to roots list and push head command */
  push(ctx->root_list, cnt);
  cnt->head = push_jump(ctx, NULL);
  /* set as hover root if the mouse is overlapping this container and it has a
  ** higher zindex than the current hover root */
  if (rect_overlaps_vec2(cnt->rect, ctx->mouse_pos) &&
      (!ctx->next_hover_root || cnt->zindex > ctx->next_hover_root->zindex)
  ) {
    ctx->next_hover_root = cnt;
  }
  /* clipping is reset here in case a root-container is made within
  ** another root-containers's begin/end block; this prevents the inner
  ** root-container being clipped to the outer */
  push(ctx->clip_stack, unclipped_rect);
}


/**
*	@brief\tEnd a root container and patch its tail jump command.
*	@param ctx Active context.
*/
static void end_root_container(mu_Context *ctx) {
  /* push tail 'goto' jump command and set head 'skip' command. the final steps
  ** on initing these are done in mu_end() */
  mu_Container *cnt = mu_get_current_container(ctx);
  cnt->tail = push_jump(ctx, NULL);
  cnt->head->jump.dst = ctx->command_list.items + ctx->command_list.idx;
  /* pop base clip rect and container */
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
}


/**
*	@brief\tBegin a standard window container.
*	@param ctx Active context.
*	@param title Window title.
*	@param rect Initial window rectangle.
*	@param opt Window option bitmask.
*	@return Widget result flags.
*/
int mu_begin_window_ex(mu_Context *ctx, const char *title, mu_Rect rect, int opt) {
  mu_Rect body;
  mu_Id id = mu_get_id(ctx, title, strlen(title));
  mu_Container *cnt = get_container(ctx, id, opt);
  if (!cnt || !cnt->open) { return 0; }
  push(ctx->id_stack, id);

  if (cnt->rect.w == 0) { cnt->rect = rect; }
  begin_root_container(ctx, cnt);
  rect = body = cnt->rect;

  /* draw frame */
  if (~opt & MU_OPT_NOFRAME) {
    ctx->draw_frame(ctx, rect, MU_COLOR_WINDOWBG);
  }

  /* do title bar */
  if (~opt & MU_OPT_NOTITLE) {
    mu_Rect tr = rect;
    tr.h = ctx->style->title_height;
    // Draw the titlebar using the active color only for the top-most window
    // so the title state follows the actual input-active container, not just
    // the last focused widget id.
    if ( cnt->zindex == ctx->last_zindex ) {
        ctx->draw_frame( ctx, tr, MU_COLOR_TITLEBG_ACTIVE );
    } else {
        ctx->draw_frame( ctx, tr, MU_COLOR_TITLEBG_INACTIVE );
    }

    /* do title text */
    if (~opt & MU_OPT_NOTITLE) {
      mu_Id id = mu_get_id(ctx, "!title", 6);
      mu_update_control(ctx, id, tr, opt);
      mu_draw_control_text(ctx, title, tr, MU_COLOR_TITLETEXT, opt);
      if (id == ctx->focus && ctx->mouse_down == MU_MOUSE_LEFT) {
        cnt->rect.x += ctx->mouse_delta.x;
        cnt->rect.y += ctx->mouse_delta.y;
      }
      body.y += tr.h;
      body.h -= tr.h;
    }

    /* do `close` button */
    if (~opt & MU_OPT_NOCLOSE) {
      mu_Id id = mu_get_id(ctx, "!close", 6);
      mu_Rect r = mu_rect(tr.x + tr.w - tr.h, tr.y, tr.h, tr.h);
      tr.w -= r.w;
      mu_draw_icon(ctx, MU_ICON_CLOSE, r, ctx->style->colors[MU_COLOR_TITLETEXT]);
      mu_update_control(ctx, id, r, opt);
      if (ctx->mouse_pressed == MU_MOUSE_LEFT && id == ctx->focus) {
        cnt->open = 0;
      }
    }
  }

  push_container_body(ctx, cnt, body, opt);

  /* do `resize` handle */
  if (~opt & MU_OPT_NORESIZE) {
    int sz = ctx->style->title_height;
    mu_Id id = mu_get_id(ctx, "!resize", 7);
    mu_Rect r = mu_rect(rect.x + rect.w - sz, rect.y + rect.h - sz, sz, sz);
    mu_update_control(ctx, id, r, opt);
    if (id == ctx->focus && ctx->mouse_down == MU_MOUSE_LEFT) {
      cnt->rect.w = mu_max(96, cnt->rect.w + ctx->mouse_delta.x);
      cnt->rect.h = mu_max(64, cnt->rect.h + ctx->mouse_delta.y);
    }
  }

  /* resize to content size */
  if (opt & MU_OPT_AUTOSIZE) {
    mu_Rect r = get_layout(ctx)->body;
    cnt->rect.w = cnt->content_size.x + (cnt->rect.w - r.w);
    cnt->rect.h = cnt->content_size.y + (cnt->rect.h - r.h);
  }

  /* close if this is a popup window and elsewhere was clicked */
  if (opt & MU_OPT_POPUP && ctx->mouse_pressed && ctx->hover_root != cnt) {
    cnt->open = 0;
  }

  mu_push_clip_rect(ctx, cnt->body);
  return MU_RES_ACTIVE;
}


/**
*	@brief\tEnd the current window scope.
*	@param ctx Active context.
*/
void mu_end_window(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  end_root_container(ctx);
}


/**
*	@brief\tOpen a named popup at the mouse cursor.
*	@param ctx Active context.
*	@param name Popup name.
*/
void mu_open_popup(mu_Context *ctx, const char *name) {
  mu_Container *cnt = mu_get_container(ctx, name);
  /* set as hover root so popup isn't closed in begin_window_ex()  */
  ctx->hover_root = ctx->next_hover_root = cnt;
  /* position at mouse cursor, open and bring-to-front */
  cnt->rect = mu_rect(ctx->mouse_pos.x, ctx->mouse_pos.y, 1, 1);
  cnt->open = 1;
  mu_bring_to_front(ctx, cnt);
}


/**
*	@brief\tBegin a popup window with standard popup options.
*	@param ctx Active context.
*	@param name Popup name.
*	@return Widget result flags.
*/
int mu_begin_popup(mu_Context *ctx, const char *name) {
  int opt = MU_OPT_POPUP | MU_OPT_AUTOSIZE | MU_OPT_NORESIZE |
            MU_OPT_NOSCROLL | MU_OPT_NOTITLE | MU_OPT_CLOSED;
  return mu_begin_window_ex(ctx, name, mu_rect(0, 0, 0, 0), opt);
}


/**
*	@brief\tEnd the current popup scope.
*	@param ctx Active context.
*/
void mu_end_popup(mu_Context *ctx) {
  mu_end_window(ctx);
}


/**
*	@brief\tBegin a retained panel widget.
*	@param ctx Active context.
*	@param name Panel name.
*	@param opt Panel option bitmask.
*/
void mu_begin_panel_ex(mu_Context *ctx, const char *name, int opt) {
  mu_Container *cnt;
  mu_push_id(ctx, name, strlen(name));
  cnt = get_container(ctx, ctx->last_id, opt);
  cnt->rect = mu_layout_next(ctx);
  if (~opt & MU_OPT_NOFRAME) {
    ctx->draw_frame(ctx, cnt->rect, MU_COLOR_PANELBG);
  }
  push(ctx->container_stack, cnt);
  push_container_body(ctx, cnt, cnt->rect, opt);
  mu_push_clip_rect(ctx, cnt->body);
}


/**
*	@brief\tEnd the current panel scope.
*	@param ctx Active context.
*/
void mu_end_panel(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
}
