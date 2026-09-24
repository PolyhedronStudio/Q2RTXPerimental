/*
Copyright (C) 2003-2006 Andrey Nazarov

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "gl.h"

drawStatic_t draw;

static inline void _GL_StretchPic(
    float x, float y, float w, float h,
    float s1, float t1, float s2, float t2,
    uint32_t color, int texnum, int flags)
{
    vec_t *dst_vert;
    uint32_t *dst_color;
    QGL_INDEX_TYPE *dst_indices;

    if (tess.numverts + 4 > TESS_MAX_VERTICES ||
        tess.numindices + 6 > TESS_MAX_INDICES ||
        (tess.numverts && tess.texnum[0] != texnum)) {
        GL_Flush2D();
    }

    tess.texnum[0] = texnum;

    dst_vert = tess.vertices + tess.numverts * 4;
    Vector4Set(dst_vert,      x,     y,     s1, t1);
    Vector4Set(dst_vert +  4, x + w, y,     s2, t1);
    Vector4Set(dst_vert +  8, x + w, y + h, s2, t2);
    Vector4Set(dst_vert + 12, x,     y + h, s1, t2);

    dst_color = (uint32_t *)tess.colors + tess.numverts;
    dst_color[0] = color;
    dst_color[1] = color;
    dst_color[2] = color;
    dst_color[3] = color;

    dst_indices = tess.indices + tess.numindices;
    dst_indices[0] = tess.numverts + 0;
    dst_indices[1] = tess.numverts + 2;
    dst_indices[2] = tess.numverts + 3;
    dst_indices[3] = tess.numverts + 0;
    dst_indices[4] = tess.numverts + 1;
    dst_indices[5] = tess.numverts + 2;

    if (flags & IF_TRANSPARENT) {
        if ((flags & IF_PALETTED) && draw.scale == 1) {
            tess.flags |= 1;
        } else {
            tess.flags |= 2;
        }
    }

    if ((color & U32_ALPHA) != U32_ALPHA) {
        tess.flags |= 2;
    }

    tess.numverts += 4;
    tess.numindices += 6;
}

#define GL_StretchPic(x,y,w,h,s1,t1,s2,t2,color,image) \
    _GL_StretchPic(x,y,w,h,s1,t1,s2,t2,color,(image)->texnum,(image)->flags)

void GL_Blend(void)
{
    color_t color;

    color.u8[0] = glr.fd.screen_blend[0] * 255;
    color.u8[1] = glr.fd.screen_blend[1] * 255;
    color.u8[2] = glr.fd.screen_blend[2] * 255;
    color.u8[3] = glr.fd.screen_blend[3] * 255;

    _GL_StretchPic(glr.fd.x, glr.fd.y, glr.fd.width, glr.fd.height, 0, 0, 1, 1,
                   color.u32, TEXNUM_WHITE, 0);
}

void R_ClearColor_GL(void)
{
    draw.colors[0].u32 = U32_WHITE;
    draw.colors[1].u32 = U32_WHITE;
}

void R_SetAlpha_GL(float alpha)
{
    draw.colors[0].u8[3] =
        draw.colors[1].u8[3] = alpha * 255;
}

void R_SetAlphaScale_GL(float alpha)
{
	// nop - only used by the RTX renderer
}

void R_SetColor_GL(uint32_t color)
{
    draw.colors[0].u32 = color;
    draw.colors[1].u8[3] = draw.colors[0].u8[3];
}

void R_SetClipRect_GL(const clipRect_t *clip)
{
    clipRect_t rc;
    float scale;

    GL_Flush2D();

    if (!clip) {
clear:
        if (draw.scissor) {
            qglDisable(GL_SCISSOR_TEST);
            draw.scissor = false;
        }
        return;
    }

    scale = 1 / draw.scale;

    rc.left = clip->left * scale;
    rc.top = clip->top * scale;
    rc.right = clip->right * scale;
    rc.bottom = clip->bottom * scale;

    if (rc.left < 0)
        rc.left = 0;
    if (rc.top < 0)
        rc.top = 0;
    if (rc.right > r_config.width)
        rc.right = r_config.width;
    if (rc.bottom > r_config.height)
        rc.bottom = r_config.height;
    if (rc.right < rc.left)
        goto clear;
    if (rc.bottom < rc.top)
        goto clear;

    qglEnable(GL_SCISSOR_TEST);
    qglScissor(rc.left, r_config.height - rc.bottom,
               rc.right - rc.left, rc.bottom - rc.top);
    draw.scissor = true;
}

#include "refresh/fonts_mtsdf.h"

void R_SetScale_GL(float scale)
{
    if (draw.scale == scale) {
        return;
    }

    GL_Flush2D();

    GL_Ortho(0, Q_rint(r_config.width * scale),
             Q_rint(r_config.height * scale), 0, -1, 1);

    draw.scale = scale;
}

/**
*	@brief	Set stroke color and uniform thickness across all four edges for GL.
**/
void R_SetStroke_GL( const uint32_t color, const float thickness ) {
	if ( thickness <= 0.0f ) {
		draw.style_flags &= ~STYLE_FLAG_OUTLINE;
		draw.stroke_thickness[ 0 ] = draw.stroke_thickness[ 1 ] = draw.stroke_thickness[ 2 ] = draw.stroke_thickness[ 3 ] = 0.0f;
		return;
	}
	draw.style_flags |= STYLE_FLAG_OUTLINE;
	for ( int32_t i = 0; i < 4; i++ ) {
		draw.stroke_colors[ i ] = color;
		draw.stroke_thickness[ i ] = thickness;
	}
}

/**
*	@brief	Set stroke thickness uniformly across all four edges for GL.
**/
void R_SetStrokeThickness_GL( const float thickness ) {
	if ( thickness <= 0.0f ) {
		draw.style_flags &= ~STYLE_FLAG_OUTLINE;
	} else {
		draw.style_flags |= STYLE_FLAG_OUTLINE;
	}
	for ( int32_t i = 0; i < 4; i++ ) {
		draw.stroke_thickness[ i ] = max( 0.0f, thickness );
	}
}

/**
*	@brief	Set stroke thickness individually per edge for GL.
**/
void R_SetStrokeThickness4_GL( const float top, const float right, const float bottom, const float left ) {
	draw.stroke_thickness[ 0 ] = max( 0.0f, top );
	draw.stroke_thickness[ 1 ] = max( 0.0f, right );
	draw.stroke_thickness[ 2 ] = max( 0.0f, bottom );
	draw.stroke_thickness[ 3 ] = max( 0.0f, left );
	if ( top > 0.0f || right > 0.0f || bottom > 0.0f || left > 0.0f ) {
		draw.style_flags |= STYLE_FLAG_OUTLINE;
	} else {
		draw.style_flags &= ~STYLE_FLAG_OUTLINE;
	}
}

/**
*	@brief	Set stroke outline colors individually per edge for GL.
**/
void R_SetStrokeColors4_GL( const uint32_t top, const uint32_t right, const uint32_t bottom, const uint32_t left ) {
	draw.stroke_colors[ 0 ] = top;
	draw.stroke_colors[ 1 ] = right;
	draw.stroke_colors[ 2 ] = bottom;
	draw.stroke_colors[ 3 ] = left;
}

/**
*	@brief	Set stroke outline with custom alignment flags for GL.
**/
void R_SetStrokeEx_GL( const uint32_t color, const float thickness, const uint32_t flags ) {
	R_SetStroke_GL( color, thickness );
	draw.style_flags = ( draw.style_flags & ~STYLE_FLAG_STROKE_ALIGN_MASK ) | ( flags & STYLE_FLAG_STROKE_ALIGN_MASK );
}

/**
*	@brief	Set stroke outline colors and thicknesses per edge for GL.
**/
void R_SetStroke4Ex_GL( const uint32_t colors[ 4 ], const float thickness[ 4 ], const uint32_t flags ) {
	if ( colors != NULL && thickness != NULL ) {
		R_SetStrokeColors4_GL( colors[ 0 ], colors[ 1 ], colors[ 2 ], colors[ 3 ] );
		R_SetStrokeThickness4_GL( thickness[ 0 ], thickness[ 1 ], thickness[ 2 ], thickness[ 3 ] );
	}
	draw.style_flags = ( draw.style_flags & ~STYLE_FLAG_STROKE_ALIGN_MASK ) | ( flags & STYLE_FLAG_STROKE_ALIGN_MASK );
}

/**
*	@brief	Set outer glow color and uniform radius across all edges for GL.
**/
void R_SetOuterGlow_GL( const uint32_t color, const float radius ) {
	if ( radius <= 0.0f ) {
		draw.style_flags &= ~STYLE_FLAG_OUTER_GLOW;
		draw.outer_glow_radius[ 0 ] = draw.outer_glow_radius[ 1 ] = draw.outer_glow_radius[ 2 ] = draw.outer_glow_radius[ 3 ] = 0.0f;
		return;
	}
	draw.style_flags |= ( STYLE_FLAG_OUTER_GLOW | STYLE_FLAG_EDGE_ALL );
	for ( int32_t i = 0; i < 4; i++ ) {
		draw.outer_glow_colors[ i ] = color;
		draw.outer_glow_radius[ i ] = radius;
	}
}

/**
*	@brief	Set outer glow radius individually per edge for GL.
**/
void R_SetOuterGlowRadius4_GL( const float top, const float right, const float bottom, const float left ) {
	draw.outer_glow_radius[ 0 ] = max( 0.0f, top );
	draw.outer_glow_radius[ 1 ] = max( 0.0f, right );
	draw.outer_glow_radius[ 2 ] = max( 0.0f, bottom );
	draw.outer_glow_radius[ 3 ] = max( 0.0f, left );
	if ( top > 0.0f || right > 0.0f || bottom > 0.0f || left > 0.0f ) {
		draw.style_flags |= STYLE_FLAG_OUTER_GLOW;
	} else {
		draw.style_flags &= ~STYLE_FLAG_OUTER_GLOW;
	}
}

/**
*	@brief	Set outer glow colors individually per edge for GL.
**/
void R_SetOuterGlowColors4_GL( const uint32_t top, const uint32_t right, const uint32_t bottom, const uint32_t left ) {
	draw.outer_glow_colors[ 0 ] = top;
	draw.outer_glow_colors[ 1 ] = right;
	draw.outer_glow_colors[ 2 ] = bottom;
	draw.outer_glow_colors[ 3 ] = left;
}

/**
*	@brief	Set which edges emit outer glow for GL.
**/
void R_SetOuterGlowEdges_GL( const uint32_t edge_mask ) {
	draw.style_flags = ( draw.style_flags & ~STYLE_FLAG_EDGE_ALL ) | ( edge_mask & STYLE_FLAG_EDGE_ALL );
}

/**
*	@brief	Set outer glow with custom falloff flags for GL.
**/
void R_SetOuterGlowEx_GL( const uint32_t color, const float radius, const uint32_t flags ) {
	R_SetOuterGlow_GL( color, radius );
	const uint32_t glow_mask = ( STYLE_FLAG_GLOW_FALLOFF_EXP | STYLE_FLAG_GLOW_BLEND_ADDITIVE | STYLE_FLAG_EDGE_ALL );
	draw.style_flags = ( draw.style_flags & ~glow_mask ) | ( flags & glow_mask );
}

/**
*	@brief	Set outer glow colors and radii per edge for GL.
**/
void R_SetOuterGlow4Ex_GL( const uint32_t colors[ 4 ], const float radii[ 4 ], const uint32_t flags ) {
	if ( colors != NULL && radii != NULL ) {
		R_SetOuterGlowColors4_GL( colors[ 0 ], colors[ 1 ], colors[ 2 ], colors[ 3 ] );
		R_SetOuterGlowRadius4_GL( radii[ 0 ], radii[ 1 ], radii[ 2 ], radii[ 3 ] );
	}
	const uint32_t glow_mask = ( STYLE_FLAG_GLOW_FALLOFF_EXP | STYLE_FLAG_GLOW_BLEND_ADDITIVE | STYLE_FLAG_EDGE_ALL );
	draw.style_flags = ( draw.style_flags & ~glow_mask ) | ( flags & glow_mask );
}

/**
*	@brief	Set inner glow color and radius for GL.
**/
void R_SetInnerGlow_GL( const uint32_t color, const float radius ) {
	if ( radius <= 0.0f ) {
		draw.style_flags &= ~STYLE_FLAG_INNER_GLOW;
		draw.inner_glow_radius[ 0 ] = draw.inner_glow_radius[ 1 ] = draw.inner_glow_radius[ 2 ] = draw.inner_glow_radius[ 3 ] = 0.0f;
		return;
	}
	draw.style_flags |= STYLE_FLAG_INNER_GLOW;
	for ( int32_t i = 0; i < 4; i++ ) {
		draw.inner_glow_colors[ i ] = color;
		draw.inner_glow_radius[ i ] = radius;
	}
}

/**
*	@brief	Set inner glow colors individually per edge for GL.
**/
void R_SetInnerGlowColors4_GL( const uint32_t top, const uint32_t right, const uint32_t bottom, const uint32_t left ) {
	draw.inner_glow_colors[ 0 ] = top;
	draw.inner_glow_colors[ 1 ] = right;
	draw.inner_glow_colors[ 2 ] = bottom;
	draw.inner_glow_colors[ 3 ] = left;
}

/**
*	@brief	Set inner glow radius individually per edge for GL.
**/
void R_SetInnerGlowRadius4_GL( const float top, const float right, const float bottom, const float left ) {
	draw.inner_glow_radius[ 0 ] = max( 0.0f, top );
	draw.inner_glow_radius[ 1 ] = max( 0.0f, right );
	draw.inner_glow_radius[ 2 ] = max( 0.0f, bottom );
	draw.inner_glow_radius[ 3 ] = max( 0.0f, left );
	if ( top > 0.0f || right > 0.0f || bottom > 0.0f || left > 0.0f ) {
		draw.style_flags |= STYLE_FLAG_INNER_GLOW;
	} else {
		draw.style_flags &= ~STYLE_FLAG_INNER_GLOW;
	}
}

/**
*	@brief	Set inner glow with custom modifier flags for GL.
**/
void R_SetInnerGlowEx_GL( const uint32_t color, const float radius, const uint32_t flags ) {
	R_SetInnerGlow_GL( color, radius );
	const uint32_t glow_mask = ( STYLE_FLAG_GLOW_FALLOFF_EXP | STYLE_FLAG_GLOW_BLEND_ADDITIVE );
	draw.style_flags = ( draw.style_flags & ~glow_mask ) | ( flags & glow_mask );
}

/**
*	@brief	Set uniform corner radius for GL.
**/
void R_SetCornerRadius_GL( const float radius ) {
	if ( radius <= 0.0f ) {
		draw.style_flags &= ~STYLE_FLAG_CORNER_RADIUS;
		draw.corner_radii[ 0 ] = draw.corner_radii[ 1 ] = draw.corner_radii[ 2 ] = draw.corner_radii[ 3 ] = 0.0f;
		return;
	}
	draw.style_flags |= STYLE_FLAG_CORNER_RADIUS;
	for ( int32_t i = 0; i < 4; i++ ) {
		draw.corner_radii[ i ] = radius;
	}
}

/**
*	@brief	Set corner radii individually for GL.
**/
void R_SetCornerRadius4_GL( const float top_left, const float top_right, const float bottom_right, const float bottom_left ) {
	draw.corner_radii[ 0 ] = max( 0.0f, top_left );
	draw.corner_radii[ 1 ] = max( 0.0f, top_right );
	draw.corner_radii[ 2 ] = max( 0.0f, bottom_right );
	draw.corner_radii[ 3 ] = max( 0.0f, bottom_left );
	if ( top_left > 0.0f || top_right > 0.0f || bottom_right > 0.0f || bottom_left > 0.0f ) {
		draw.style_flags |= STYLE_FLAG_CORNER_RADIUS;
	} else {
		draw.style_flags &= ~STYLE_FLAG_CORNER_RADIUS;
	}
}

/**
*	@brief	Reset all 2D styling to default for GL.
**/
void R_ClearStyle_GL( void ) {
	draw.style_flags = STYLE_FLAG_NONE;
	for ( int32_t i = 0; i < 4; i++ ) {
		draw.stroke_colors[ i ] = 0;
		draw.stroke_thickness[ i ] = 0.0f;
		draw.outer_glow_colors[ i ] = 0;
		draw.outer_glow_radius[ i ] = 0.0f;
		draw.inner_glow_colors[ i ] = 0;
		draw.inner_glow_radius[ i ] = 0.0f;
		draw.corner_radii[ i ] = 0.0f;
	}
}

/**
*	@brief	Draw a 2D line segment fallback for GL.
*	@param	x1			Start X coordinate.
*	@param	y1			Start Y coordinate.
*	@param	x2			End X coordinate.
*	@param	y2			End Y coordinate.
*	@param	thickness	Line thickness in pixels.
*	@param	color		Packed RGBA color.
**/
void R_DrawLine2D_GL( const float x1, const float y1, const float x2, const float y2, const float thickness, const uint32_t color ) {
	const float dx = x2 - x1;
	const float dy = y2 - y1;
	const float min_x = min( x1, x2 );
	const float min_y = min( y1, y2 );
	const float w = max( thickness, fabsf( dx ) );
	const float h = max( thickness, fabsf( dy ) );
	R_DrawFill32( (int)min_x, (int)min_y, (int)w, (int)h, color );
}

/**
*	@brief	Register a TrueType font and generate MTSDF atlas for GL.
*	@param	path			Relative path to the font asset file.
*	@param	pixel_height	Nominal raster height for the font in pixels.
*	@return	Image handle to the registered font atlas, or 0 on failure.
**/
qhandle_t R_RegisterFontTTF_GL( const char *path, const float pixel_height ) {
	return R_RegisterFontTTF_Impl( path, pixel_height );
}

/**
*	@brief	Draw 3D occluded world text fallback for GL.
*	@param	origin	World-space 3D origin (X, Y, Z).
*	@param	angles	Pitch, Yaw, Roll orientation in degrees, or nullptr for camera-facing billboard.
*	@param	scale	Character height in Quake world units.
*	@param	text	String to render.
*	@param	font	Font handle (TrueType MTSDF font or legacy bitmap font).
*	@param	color	Packed RGBA color tint.
**/
void R_DrawString3DOccluded_GL( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color ) {
	// Fallback in legacy GL
}

/**
*	@brief	Draw 3D non-occluded world text fallback for GL.
*	@param	origin	World-space 3D origin (X, Y, Z).
*	@param	angles	Pitch, Yaw, Roll orientation in degrees, or nullptr for camera-facing billboard.
*	@param	scale	Character height in Quake world units.
*	@param	text	String to render.
*	@param	font	Font handle (TrueType MTSDF font or legacy bitmap font).
*	@param	color	Packed RGBA color tint.
**/
void R_DrawString3DNonOccluded_GL( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color ) {
	// Fallback in legacy GL
}

/**
*	@brief	Draw 3D world text fallback for GL.
*	@param	origin	World-space 3D origin (X, Y, Z).
*	@param	angles	Pitch, Yaw, Roll orientation in degrees, or nullptr for camera-facing billboard.
*	@param	scale	Character height in Quake world units.
*	@param	text	String to render.
*	@param	font	Font handle (TrueType MTSDF font or legacy bitmap font).
*	@param	color	Packed RGBA color tint.
**/
void R_DrawString3D_GL( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color ) {
	// Fallback in legacy GL
}

void R_DrawStretchPic_GL(int x, int y, int w, int h, qhandle_t pic)
{
    image_t *image = IMG_ForHandle(pic);

    GL_StretchPic(x, y, w, h, image->sl, image->tl, image->sh, image->th,
                  draw.colors[0].u32, image);
}
void R_DrawRotateStretchPic_GL( int x, int y, int w, int h, float angle, int pivot_x, int pivot_y, qhandle_t pic ) {
    // WID: TODO: Implement?
}

void R_DrawKeepAspectPic_GL(int x, int y, int w, int h, qhandle_t pic)
{
    image_t *image = IMG_ForHandle(pic);

    if (image->flags & IF_SCRAP) {
        R_DrawStretchPic(x, y, w, h, pic);
        return;
    }

    float scale_w = w;
    float scale_h = h * image->aspect;
    float scale = max(scale_w, scale_h);

    float s = (1.0f - scale_w / scale) * 0.5f;
    float t = (1.0f - scale_h / scale) * 0.5f;

    GL_StretchPic(x, y, w, h, s, t, 1.0f - s, 1.0f - t, draw.colors[0].u32, image);
}

void R_DrawPic_GL(int x, int y, qhandle_t pic)
{
    image_t *image = IMG_ForHandle(pic);

    GL_StretchPic(x, y, image->width, image->height,
                  image->sl, image->tl, image->sh, image->th, draw.colors[0].u32, image);
}

void
R_DrawPicEx_GL( double destX, double destY, double destW, double destH, qhandle_t pic,
    double srcX, double srcY, double srcW, double srcH ) {
    image_t *image = IMG_ForHandle( pic );

    //GL_StretchPic( x, y, image->width, image->height,
    //    image->sl, image->tl, image->sh, image->th, draw.colors[ 0 ].u32, image );

    double s0 = srcX / image->width;
    double t0 = srcY / image->height;
    double s1 = ( srcX + srcW ) / image->width;
    double t1 = ( srcY + srcH ) / image->height;

    GL_StretchPic( destX, destY, destW, destH,
        s0, t0, s1, t1, draw.colors[ 0 ].u32, image );
}

void R_DrawStretchRaw_GL(int x, int y, int w, int h)
{
    _GL_StretchPic(x, y, w, h, 0, 0, 1, 1, U32_WHITE, TEXNUM_RAW, 0);
}

void R_UpdateRawPic_GL(int pic_w, int pic_h, const uint32_t *pic)
{
    GL_ForceTexture(0, TEXNUM_RAW);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pic_w, pic_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pic);
}

void R_DiscardRawPic_GL(void)
{
    // Do nothing
}

#define DIV64 (1.0f / 64.0f)

void R_TileClear_GL(int x, int y, int w, int h, qhandle_t pic)
{
    GL_StretchPic(x, y, w, h, x * DIV64, y * DIV64,
                  (x + w) * DIV64, (y + h) * DIV64, U32_WHITE, IMG_ForHandle(pic));
}

void R_DrawFill8_GL(int x, int y, int w, int h, int c)
{
    if (!w || !h)
        return;
    _GL_StretchPic(x, y, w, h, 0, 0, 1, 1, d_8to24table[c & 0xff], TEXNUM_WHITE, 0);
}

void R_DrawFill32_GL(int x, int y, int w, int h, uint32_t color)
{
    if (!w || !h)
        return;
    _GL_StretchPic(x, y, w, h, 0, 0, 1, 1, color, TEXNUM_WHITE, 0);
}

void R_DrawFill8f_GL( float x, float y, float w, float h, int c ) {
    if ( !w || !h )
        return;
    _GL_StretchPic( x, y, w, h, 0, 0, 1, 1, d_8to24table[ c & 0xff ], TEXNUM_WHITE, 0 );
}

void R_DrawFill32f_GL( float x, float y, float w, float h, uint32_t color ) {
    if ( !w || !h )
        return;
    _GL_StretchPic( x, y, w, h, 0, 0, 1, 1, color, TEXNUM_WHITE, 0 );
}

static inline void draw_char(int x, int y, int flags, int c, image_t *image)
{
    float s, t;

    if ((c & 127) == 32) {
        return;
    }

    if (flags & UI_ALTCOLOR) {
        c |= 0x80;
    }
    if (flags & UI_XORCOLOR) {
        c ^= 0x80;
    }

    s = (c & 15) * 0.0625f;
    t = (c >> 4) * 0.0625f;

    if (gl_fontshadow->integer > 0 && c != 0x83) {
        uint32_t black = MakeColor(0, 0, 0, draw.colors[0].u8[3]);

        GL_StretchPic(x + 1, y + 1, CHAR_WIDTH, CHAR_HEIGHT, s, t,
                      s + 0.0625f, t + 0.0625f, black, image);

        if (gl_fontshadow->integer > 1)
            GL_StretchPic(x + 2, y + 2, CHAR_WIDTH, CHAR_HEIGHT, s, t,
                          s + 0.0625f, t + 0.0625f, black, image);
    }

    GL_StretchPic(x, y, CHAR_WIDTH, CHAR_HEIGHT, s, t,
                  s + 0.0625f, t + 0.0625f, draw.colors[c >> 7].u32, image);
}

void R_DrawChar_GL(int x, int y, int flags, int c, qhandle_t font)
{
    draw_char(x, y, flags, c & 255, IMG_ForHandle(font));
}

int R_DrawString_GL(int x, int y, int flags, size_t maxlen, const char *s, qhandle_t font)
{
    image_t *image = IMG_ForHandle(font);

    while (maxlen-- && *s) {
        byte c = *s++;
        draw_char(x, y, flags, c, image);
        x += CHAR_WIDTH;
    }

    return x;
}

#if USE_DEBUG

qhandle_t r_charset;

static void Draw_Stringf(int x, int y, const char *fmt, ...)
{
    va_list argptr;
    char buffer[MAX_STRING_CHARS];

    va_start(argptr, fmt);
    Q_vsnprintf(buffer, sizeof(buffer), fmt, argptr);
    va_end(argptr);

    R_DrawString(x, y, 0, -1, buffer, r_charset);
}

extern int get_auto_scale(void);

void Draw_Stats(void)
{
    int x = 10, y = 10;

    R_SetScale(1.0f / get_auto_scale());
    R_DrawFill8(8, 8, 24*8, 20*10+2, 4);

    Draw_Stringf(x, y, "Nodes visible  : %i", c.nodesVisible); y += 10;
    Draw_Stringf(x, y, "Nodes culled   : %i", c.nodesCulled); y += 10;
    Draw_Stringf(x, y, "Nodes drawn    : %i", c.nodesDrawn); y += 10;
    Draw_Stringf(x, y, "Leaves drawn   : %i", c.leavesDrawn); y += 10;
    Draw_Stringf(x, y, "Faces drawn    : %i", c.facesDrawn); y += 10;
    Draw_Stringf(x, y, "Faces culled   : %i", c.facesCulled); y += 10;
    Draw_Stringf(x, y, "Boxes culled   : %i", c.boxesCulled); y += 10;
    Draw_Stringf(x, y, "Spheres culled : %i", c.spheresCulled); y += 10;
    Draw_Stringf(x, y, "RtBoxes culled : %i", c.rotatedBoxesCulled); y += 10;
    Draw_Stringf(x, y, "Tris drawn     : %i", c.trisDrawn); y += 10;
    Draw_Stringf(x, y, "Tex switches   : %i", c.texSwitches); y += 10;
    Draw_Stringf(x, y, "Tex uploads    : %i", c.texUploads); y += 10;
    Draw_Stringf(x, y, "Batches drawn  : %i", c.batchesDrawn); y += 10;
    Draw_Stringf(x, y, "Faces / batch  : %.1f", c.batchesDrawn ? (float)c.facesDrawn / c.batchesDrawn : 0.0f); y += 10;
    Draw_Stringf(x, y, "Tris / batch   : %.1f", c.batchesDrawn ? (float)c.facesTris / c.batchesDrawn : 0.0f); y += 10;
    Draw_Stringf(x, y, "2D batches     : %i", c.batchesDrawn2D); y += 10;
    Draw_Stringf(x, y, "Total entities : %i", glr.fd.num_entities); y += 10;
    Draw_Stringf(x, y, "Total dlights  : %i", glr.fd.num_dlights); y += 10;
    Draw_Stringf(x, y, "Total particles: %i", glr.fd.num_particles); y += 10;
    Draw_Stringf(x, y, "Uniform uploads: %i", c.uniformUploads); y += 10;

    R_SetScale(1.0f);
}

void Draw_Lightmaps(void)
{
    int i, x, y;

    for (i = 0; i < lm.nummaps; i++) {
        x = i & 1;
        y = i >> 1;
        _GL_StretchPic(256 * x, 256 * y, 256, 256,
                       0, 0, 1, 1, U32_WHITE, lm.texnums[i], 0);
    }
}

void Draw_Scrap(void)
{
    _GL_StretchPic(0, 0, 256, 256,
                   0, 0, 1, 1, U32_WHITE, TEXNUM_SCRAP, IF_PALETTED | IF_TRANSPARENT);
}

#endif
