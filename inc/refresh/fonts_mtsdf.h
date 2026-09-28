/********************************************************************
*
*
*	Refresh: TrueType MTSDF (Multi-Channel Signed Distance Field)
*	Font Loader and Runtime Atlas Generator.
*
*	Generates 4-channel MTSDF texture atlases (RGBA8) from TrueType
*	and OpenType fonts using stb_truetype and stb_rect_pack:
*	- RGB: Multi-channel signed distance field for razor-sharp corners.
*	- Alpha: True Euclidean distance for isotropic glows and shadows.
*
*
********************************************************************/
#pragma once

#include "shared/shared.h"
#include "refresh/refresh.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Maximum number of glyphs tracked per MTSDF font (standard ASCII + extended Latin).
#define MTSDF_MAX_GLYPHS 256

//! Maximum number of simultaneously loaded MTSDF TrueType fonts.
#define MTSDF_MAX_FONTS 16

//! Number of padding pixels placed around each glyph in the atlas for distance spread.
#define MTSDF_GLYPH_PADDING 8

//! Reference distance spread range in pixels stored in the distance field.
#define MTSDF_PIXEL_RANGE 8.0f

/**
*	@brief	Glyph metrics and UV mapping within a packed MTSDF font atlas.
**/
typedef struct font_glyph_mtsdf_s {
	int32_t		char_code;		//!< ASCII or Unicode code point
	float		advance;		//!< Horizontal advance width in unscaled font units
	float		bearing_x;		//!< Left side bearing (horizontal offset from origin to bounding box)
	float		bearing_y;		//!< Top side bearing (vertical offset from baseline to top of glyph)
	float		width;			//!< Width of glyph geometry in unscaled font units
	float		height;			//!< Height of glyph geometry in unscaled font units
	float		s0, t0;			//!< Top-left normalized texture coordinates in the atlas [0, 1]
	float		s1, t1;			//!< Bottom-right normalized texture coordinates in the atlas [0, 1]
} font_glyph_mtsdf_t;

/**
*	@brief	MTSDF TrueType font descriptor containing atlas handle and font metrics.
**/
typedef struct font_mtsdf_s {
	char				path[ MAX_QPATH ];				//!< Virtual filesystem path to font file
	float				pixel_height;					//!< Target nominal rasterization height in pixels
	qhandle_t			atlas_image;					//!< Registered engine image handle for the RGBA8 atlas
	int32_t				atlas_width;					//!< Atlas texture width in pixels
	int32_t				atlas_height;					//!< Atlas texture height in pixels
	float				ascent;							//!< Font ascent above baseline in unscaled units
	float				descent;						//!< Font descent below baseline in unscaled units
	float				line_gap;						//!< Line gap between baselines in unscaled units
	float				sdf_pixel_range;				//!< Pixel spread range used for distance decoding
	font_glyph_mtsdf_t	glyphs[ MTSDF_MAX_GLYPHS ];		//!< Per-character glyph metric table
	bool				glyph_valid[ MTSDF_MAX_GLYPHS ];//!< True if glyph was successfully generated and packed
} font_mtsdf_t;

/**
*	@brief	Register and generate a 4-channel MTSDF font from a TrueType / OpenType file.
*	@param	path			Virtual file path to the font file.
*	@param	pixel_height	Reference pixel height for glyph rasterization.
*	@return	Image handle to the registered MTSDF font atlas, or 0 on failure.
**/
qhandle_t R_RegisterFontTTF_Impl( const char *path, const float pixel_height );

/**
*	@brief	Query the MTSDF font descriptor associated with a font handle.
*	@param	font	Font handle returned by R_RegisterFontTTF.
*	@return	Pointer to the font_mtsdf_t descriptor, or nullptr if not an MTSDF font.
**/
const font_mtsdf_t *Font_GetDescriptorTTF( const qhandle_t font );

/**
*	@brief	Measure string width in pixels using TrueType metrics if available.
*	@param	font	Font handle.
*	@param	text	String to measure.
*	@return	Width in pixels, falling back to standard CHAR_WIDTH * length if not TTF.
**/
float Font_StringWidthTTF( const qhandle_t font, const char *text );

/**
*	@brief	Measure string width in pixels up to maxlen characters using TrueType metrics if available.
*	@param	font	Font handle.
*	@param	text	String to measure.
*	@param	maxlen	Maximum number of characters to measure.
*	@return	Width in pixels, falling back to standard CHAR_WIDTH * length if not TTF.
**/
float Font_StringWidthTTF_N( const qhandle_t font, const char *text, const size_t maxlen );

/**
*	@brief	Query the line height of an MTSDF font in pixels.
*	@param	font	Font handle.
*	@return	Line height in pixels, or standard CHAR_HEIGHT if not TTF.
**/
float Font_GetHeightTTF( const qhandle_t font );

/**
*	@brief	Load a pregenerated MTSDF font from a binary cache file (.mtsdf).
*	@param	cache_path		Virtual filesystem path to the cache file (e.g. "fonts/segoeui_18.mtsdf").
*	@param	original_path	Original font path used for registration and lookup (e.g. "fonts/segoeui.ttf").
*	@param	pixel_height	Nominal raster height.
*	@return	Image handle to the registered MTSDF font atlas, or 0 on failure.
**/
qhandle_t Font_LoadMTSDF( const char *cache_path, const char *original_path, const float pixel_height );

/**
*	@brief	Serialize an MTSDF font descriptor and its atlas pixel buffer to a binary cache file (.mtsdf).
*	@param	cache_path		Virtual filesystem destination path (e.g. "fonts/segoeui_18.mtsdf").
*	@param	desc			Populated font descriptor containing metrics and glyph data.
*	@param	atlas_pixels	Raw RGBA8 atlas pixel data.
*	@return	True on successful serialization, false on write failure.
**/
bool Font_SaveMTSDF( const char *cache_path, const font_mtsdf_t *desc, const uint8_t *atlas_pixels );

#ifdef __cplusplus
}
#endif
