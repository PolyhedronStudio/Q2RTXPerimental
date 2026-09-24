//! TrueType Multi-Channel Signed Distance Field (MTSDF) font renderer and atlas generator.
/********************************************************************
*
*
*	Refresh: TrueType MTSDF Font Loader and Atlas Generator
*
*	Parses TrueType / OpenType font vectors via stb_truetype, evaluates
*	multi-channel signed distance fields (RGB for corners, Alpha for
*	Euclidean distance), packs glyph bounding boxes using stb_rect_pack,
*	and registers a consolidated RGBA8 atlas texture into the engine.
*
*
********************************************************************/
#include "refresh/fonts_mtsdf.h"
#include "refresh/images.h"
#include "shared/shared.h"
#include "common/common.h"
#include "common/zone.h"
#include "shared/util/util_strings.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#define STBTT_malloc(x,u)	((void)(u), Z_Malloc((int32_t)(x)))
#define STBTT_free(x,u)		((void)(u), Z_Free(x))
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

//! Array of actively loaded MTSDF fonts.
static font_mtsdf_t s_mtsdf_fonts[ MTSDF_MAX_FONTS ];
//! Current count of actively loaded MTSDF fonts.
static int32_t s_num_mtsdf_fonts = 0;

/**
*	@brief	Representation of an evaluated edge segment for MTSDF distance calculation.
**/
struct mtsdf_edge_t {
	enum edge_type {
		EDGE_LINE,
		EDGE_QUADRATIC
	} type;

	//! Segment start point in font space.
	float p0x, p0y;
	//! Control point for quadratic curves in font space.
	float cx, cy;
	//! Segment end point in font space.
	float p1x, p1y;
	//! Assigned edge color channel (1 = Red, 2 = Green, 3 = Blue).
	int32_t color_channel;
};

/**
*	@brief	Compute the signed distance from a 2D query point to a linear segment.
*	@param	px		Query point X coordinate.
*	@param	py		Query point Y coordinate.
*	@param	x0		Segment start X.
*	@param	y0		Segment start Y.
*	@param	x1		Segment end X.
*	@param	y1		Segment end Y.
*	@param	out_t	[out] Projection parameter along segment [0, 1].
*	@return	Unsigned Euclidean distance to the segment.
**/
static float PointToSegmentDistance( const float px, const float py, const float x0, const float y0, const float x1, const float y1, float *out_t ) {
	/**
	*	Calculate segment delta vector and squared length.
	**/
	const float dx = x1 - x0;
	const float dy = y1 - y0;
	const float len2 = ( dx * dx ) + ( dy * dy );

	/**
	*	Guard against degenerate zero-length segments.
	**/
	// Check if segment length is near zero to prevent division by zero.
	if ( len2 <= 1e-8f ) {
		// Output zero parameter if pointer provided.
		if ( out_t != nullptr ) {
			*out_t = 0.0f;
		}
		// Calculate point-to-point Euclidean distance directly.
		const float ex = px - x0;
		const float ey = py - y0;
		return std::sqrt( ( ex * ex ) + ( ey * ey ) );
	}

	/**
	*	Project query point onto segment and clamp parameter t to [0, 1].
	**/
	// Calculate projection factor t clamped to valid segment bounds [0, 1].
	const float t = std::clamp( ( ( px - x0 ) * dx + ( py - y0 ) * dy ) / len2, 0.0f, 1.0f );
	// Store parameter t if caller requested it.
	if ( out_t != nullptr ) {
		*out_t = t;
	}

	/**
	*	Evaluate closest point on segment and return Euclidean distance.
	**/
	// Compute coordinates of projected point.
	const float proj_x = x0 + t * dx;
	const float proj_y = y0 + t * dy;
	// Calculate delta vector from query point to projected point.
	const float rx = px - proj_x;
	const float ry = py - proj_y;
	// Return Euclidean distance.
	return std::sqrt( ( rx * rx ) + ( ry * ry ) );
}

/**
*	@brief	Subdivide and compute distance from a 2D point to a quadratic Bézier segment.
*	@param	px	Query point X coordinate.
*	@param	py	Query point Y coordinate.
*	@param	x0	Start point X.
*	@param	y0	Start point Y.
*	@param	cx	Control point X.
*	@param	cy	Control point Y.
*	@param	x1	End point X.
*	@param	y1	End point Y.
*	@return	Approximate Euclidean distance to the curve.
**/
static float PointToQuadraticDistance( const float px, const float py, const float x0, const float y0, const float cx, const float cy, const float x1, const float y1 ) {
	/**
	*	Sample curve along piecewise linear subdivisions for robust numerical stability.
	**/
	// Sample curve along 8 piecewise linear intervals.
	constexpr int32_t subdivisions = 8;
	float min_dist = 1e9f;
	float prev_x = x0;
	float prev_y = y0;

	/**
	*	Iterate over piecewise linear sub-segments.
	**/
	// Loop over each subdivision interval.
	for ( int32_t i = 1; i <= subdivisions; i++ ) {
		// Evaluate parametric progression factor t.
		const float t = (float)i / (float)subdivisions;
		const float it = 1.0f - t;
		// Compute point along quadratic Bézier curve at parameter t.
		const float cur_x = ( it * it * x0 ) + ( 2.0f * it * t * cx ) + ( t * t * x1 );
		const float cur_y = ( it * it * y0 ) + ( 2.0f * it * t * cy ) + ( t * t * y1 );

		// Measure distance from query point to current linear sub-segment.
		float seg_t = 0.0f;
		const float d = PointToSegmentDistance( px, py, prev_x, prev_y, cur_x, cur_y, &seg_t );
		// Update minimum distance found so far.
		if ( d < min_dist ) {
			min_dist = d;
		}

		// Advance previous point for next iteration.
		prev_x = cur_x;
		prev_y = cur_y;
	}

	// Return closest evaluated distance.
	return min_dist;
}

/**
*	@brief	Decompose vector contours from stbtt_vertex into colored edge segments.
*	@param	verts		Vertex array returned by stbtt_GetGlyphShape.
*	@param	num_verts	Number of vertices in the array.
*	@param	edges		[out] Vector of decomposed colored edges.
**/
static void DecomposeAndColorEdges( const stbtt_vertex *verts, const int32_t num_verts, std::vector< mtsdf_edge_t > &edges ) {
	/**
	*	Sanity checks: ensure vertices exist.
	**/
	edges.clear();
	// Guard against null pointer or empty vertex array.
	if ( verts == nullptr || num_verts <= 0 ) {
		return;
	}

	/**
	*	Initialize contour tracking coordinates and primary edge coloring channel.
	**/
	float start_x = 0.0f;
	float start_y = 0.0f;
	float cur_x = 0.0f;
	float cur_y = 0.0f;
	int32_t active_color = 1; // 1 = Red, 2 = Green, 3 = Blue

	/**
	*	Iterate through outline vertices and construct colored segments.
	**/
	// Process each vertex command sequentially.
	for ( int32_t i = 0; i < num_verts; i++ ) {
		const stbtt_vertex &v = verts[ i ];

		// Branch on vertex outline command type.
		if ( v.type == STBTT_vmove ) {
			// Begin a new contour: switch color channel to break corner continuity across contours.
			start_x = cur_x = (float)v.x;
			start_y = cur_y = (float)v.y;
			active_color = ( ( active_color ) % 3 ) + 1;
		} else if ( v.type == STBTT_vline ) {
			// Construct linear edge segment.
			mtsdf_edge_t edge;
			edge.type = mtsdf_edge_t::EDGE_LINE;
			edge.p0x = cur_x;
			edge.p0y = cur_y;
			edge.cx = 0.0f;
			edge.cy = 0.0f;
			edge.p1x = (float)v.x;
			edge.p1y = (float)v.y;
			edge.color_channel = active_color;
			edges.push_back( edge );

			// Advance current point.
			cur_x = (float)v.x;
			cur_y = (float)v.y;
			// Alternate color channel for the next segment to avoid adjacent colors at corners.
			active_color = ( ( active_color ) % 3 ) + 1;
		} else if ( v.type == STBTT_vcurve ) {
			// Construct quadratic Bézier edge segment.
			mtsdf_edge_t edge;
			edge.type = mtsdf_edge_t::EDGE_QUADRATIC;
			edge.p0x = cur_x;
			edge.p0y = cur_y;
			edge.cx = (float)v.cx;
			edge.cy = (float)v.cy;
			edge.p1x = (float)v.x;
			edge.p1y = (float)v.y;
			edge.color_channel = active_color;
			edges.push_back( edge );

			// Advance current point.
			cur_x = (float)v.x;
			cur_y = (float)v.y;
			// Alternate color channel for next segment.
			active_color = ( ( active_color ) % 3 ) + 1;
		}
	}
}

/**
*	@brief	Generate a 4-channel MTSDF bitmap for a single glyph.
*	@param	info			Font info pointer.
*	@param	glyph_index		Glyph index.
*	@param	scale			Font pixel scale.
*	@param	pixel_range		Distance spread range in pixels.
*	@param	out_w			[out] Output bitmap width.
*	@param	out_h			[out] Output bitmap height.
*	@param	out_bearing_x	[out] Left side bearing in pixels.
*	@param	out_bearing_y	[out] Top side bearing in pixels.
*	@return	Allocated RGBA8 pixel buffer (must be freed with Z_Free), or nullptr.
**/
static uint8_t *GenerateGlyphMTSDF( const stbtt_fontinfo *info, const int32_t glyph_index, const float scale, const float pixel_range, int32_t *out_w, int32_t *out_h, float *out_bearing_x, float *out_bearing_y ) {
	/**
	*	Query subpixel bounding box of the glyph shape at target scale.
	**/
	int32_t ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
	stbtt_GetGlyphBitmapBoxSubpixel( info, glyph_index, scale, scale, 0.0f, 0.0f, &ix0, &iy0, &ix1, &iy1 );

	// Calculate unpadded pixel dimensions.
	const int32_t raw_w = ix1 - ix0;
	const int32_t raw_h = iy1 - iy0;

	/**
	*	Handle empty or invisible glyphs (e.g. whitespace).
	**/
	// Return immediately if glyph has no physical area.
	if ( raw_w <= 0 || raw_h <= 0 ) {
		*out_w = 0;
		*out_h = 0;
		*out_bearing_x = 0.0f;
		*out_bearing_y = 0.0f;
		return nullptr;
	}

	/**
	*	Calculate padded bitmap dimensions and bearings to accommodate distance spread.
	**/
	const int32_t pad = MTSDF_GLYPH_PADDING;
	const int32_t bmp_w = raw_w + ( pad * 2 );
	const int32_t bmp_h = raw_h + ( pad * 2 );

	// Store output dimensions and bearing offsets.
	*out_w = bmp_w;
	*out_h = bmp_h;
	*out_bearing_x = (float)( ix0 - pad );
	*out_bearing_y = (float)( -iy0 + pad );

	/**
	*	Retrieve vector outline shape vertices and decompose into colored edge segments.
	**/
	stbtt_vertex *verts = nullptr;
	const int32_t num_verts = stbtt_GetGlyphShape( info, glyph_index, &verts );
	// Ensure valid shape vertices were obtained.
	if ( num_verts <= 0 || verts == nullptr ) {
		if ( verts != nullptr ) {
			stbtt_FreeShape( info, verts );
		}
		return nullptr;
	}

	// Decompose contours and color adjacent edges.
	std::vector< mtsdf_edge_t > edges;
	DecomposeAndColorEdges( verts, num_verts, edges );

	/**
	*	Allocate destination RGBA8 pixel memory.
	**/
	uint8_t *pixels = (uint8_t *)Z_Malloc( bmp_w * bmp_h * 4 );
	if ( pixels == nullptr ) {
		stbtt_FreeShape( info, verts );
		return nullptr;
	}

	// Compute inverse scale for mapping screen pixels back to font units.
	const float inv_scale = ( scale > 0.0f ) ? ( 1.0f / scale ) : 1.0f;

	/**
	*	Rasterize multi-channel distance field texel by texel.
	**/
	// Loop over rows in glyph bitmap.
	for ( int32_t y = 0; y < bmp_h; y++ ) {
		// Loop over columns in current row.
		for ( int32_t x = 0; x < bmp_w; x++ ) {
			// Subpixel sample position in font coordinate space (y-upwards).
			const float sample_px = (float)( ix0 - pad + x ) + 0.5f;
			const float sample_py = (float)( iy0 - pad + y ) + 0.5f;
			const float gx = sample_px * inv_scale;
			const float gy = -sample_py * inv_scale; // Invert for TrueType font coordinate space

			// Determine if point is inside shape using winding crossings.
			const int32_t winding = stbtt__compute_crossings_x( gx, gy, num_verts, verts );
			const bool is_inside = ( winding != 0 );

			// Initialize channel distance accumulators.
			float min_d_r = 1e9f;
			float min_d_g = 1e9f;
			float min_d_b = 1e9f;
			float min_d_all = 1e9f;

			// Check distance against all colored segments.
			for ( size_t e = 0; e < edges.size(); e++ ) {
				const mtsdf_edge_t &edge = edges[ e ];
				float d = 0.0f;

				// Measure distance depending on edge segment type.
				if ( edge.type == mtsdf_edge_t::EDGE_LINE ) {
					float seg_t = 0.0f;
					d = PointToSegmentDistance( gx, gy, edge.p0x, edge.p0y, edge.p1x, edge.p1y, &seg_t );
				} else {
					d = PointToQuadraticDistance( gx, gy, edge.p0x, edge.p0y, edge.cx, edge.cy, edge.p1x, edge.p1y );
				}

				// Update overall true Euclidean distance.
				if ( d < min_d_all ) {
					min_d_all = d;
				}

				// Update channel-specific distances.
				if ( edge.color_channel == 1 && d < min_d_r ) {
					min_d_r = d;
				} else if ( edge.color_channel == 2 && d < min_d_g ) {
					min_d_g = d;
				} else if ( edge.color_channel == 3 && d < min_d_b ) {
					min_d_b = d;
				}
			}

			// Fall back to min_d_all if a channel had no nearby edges.
			if ( min_d_r > 1e8f ) { min_d_r = min_d_all; }
			if ( min_d_g > 1e8f ) { min_d_g = min_d_all; }
			if ( min_d_b > 1e8f ) { min_d_b = min_d_all; }

			// Convert font-space distances to screen pixels.
			float sd_r = min_d_r * scale * ( is_inside ? 1.0f : -1.0f );
			float sd_g = min_d_g * scale * ( is_inside ? 1.0f : -1.0f );
			float sd_b = min_d_b * scale * ( is_inside ? 1.0f : -1.0f );
			float sd_a = min_d_all * scale * ( is_inside ? 1.0f : -1.0f );

			// Map to normalized range [0.0, 1.0] where 0.5 is the contour edge.
			const float norm_r = std::clamp( 0.5f + ( sd_r / ( 2.0f * pixel_range ) ), 0.0f, 1.0f );
			const float norm_g = std::clamp( 0.5f + ( sd_g / ( 2.0f * pixel_range ) ), 0.0f, 1.0f );
			const float norm_b = std::clamp( 0.5f + ( sd_b / ( 2.0f * pixel_range ) ), 0.0f, 1.0f );
			const float norm_a = std::clamp( 0.5f + ( sd_a / ( 2.0f * pixel_range ) ), 0.0f, 1.0f );

			// Pack normalized distances into RGBA8 bytes.
			const int32_t idx = ( ( y * bmp_w ) + x ) * 4;
			pixels[ idx + 0 ] = (uint8_t)( norm_r * 255.0f );
			pixels[ idx + 1 ] = (uint8_t)( norm_g * 255.0f );
			pixels[ idx + 2 ] = (uint8_t)( norm_b * 255.0f );
			pixels[ idx + 3 ] = (uint8_t)( norm_a * 255.0f );
		}
	}

	/**
	*	Free temporary outline vertices and return completed bitmap.
	**/
	stbtt_FreeShape( info, verts );
	return pixels;
}

/**
*	@brief	Register and generate a 4-channel MTSDF font from a TrueType / OpenType file.
*	@param	path			Relative path to the TrueType / OpenType font asset.
*	@param	pixel_height	Nominal raster height for the font in pixels.
*	@return	Image handle for the consolidated atlas, or 0 on failure.
**/
qhandle_t R_RegisterFontTTF_Impl( const char *path, const float pixel_height ) {
	/**
	*	Sanity checks: validate input parameters.
	**/
	// Return immediately if font path is null or empty.
	if ( path == nullptr || path[ 0 ] == '\0' ) {
		return 0;
	}

	/**
	*	Check font cache for an existing instance with identical path and pixel height.
	**/
	// Loop through currently loaded MTSDF font descriptors.
	for ( int32_t i = 0; i < s_num_mtsdf_fonts; i++ ) {
		if ( Q_stricmp( s_mtsdf_fonts[ i ].path, path ) == 0 &&
		     std::fabs( s_mtsdf_fonts[ i ].pixel_height - pixel_height ) < 0.5f ) {
			// Found cached matching font.
			return s_mtsdf_fonts[ i ].atlas_image;
		}
	}

	/**
	*	Verify font capacity limit.
	**/
	if ( s_num_mtsdf_fonts >= MTSDF_MAX_FONTS ) {
		Com_WPrintf( "%s: Maximum MTSDF font capacity reached (%d)\n", __func__, MTSDF_MAX_FONTS );
		return 0;
	}

	/**
	*	Load font file binary data from the engine filesystem.
	**/
	void *font_buffer = nullptr;
	const int32_t file_size = FS_LoadFileEx( path, &font_buffer, FS_PATH_ANY, TAG_FILESYSTEM );
	if ( file_size <= 0 || font_buffer == nullptr ) {
		Com_WPrintf( "%s: Failed to load font file '%s'\n", __func__, path );
		return 0;
	}

	/**
	*	Initialize stb_truetype font info structure.
	**/
	// Resolve font offset and validate before initializing font info.
	int32_t font_ofs = stbtt_GetFontOffsetForIndex( (const unsigned char *)font_buffer, 0 );
	if ( font_ofs < 0 ) {
		Com_WPrintf( "%s: stbtt_GetFontOffsetForIndex failed for '%s'\n", __func__, path );
		FS_FreeFile( font_buffer );
		return 0;
	}

	stbtt_fontinfo font_info;
	if ( !stbtt_InitFont( &font_info, (const unsigned char *)font_buffer, font_ofs ) ) {
		Com_WPrintf( "%s: stbtt_InitFont failed for '%s'\n", __func__, path );
		FS_FreeFile( font_buffer );
		return 0;
	}

	/**
	*	Initialize font descriptor metrics and scaling factors.
	**/
	font_mtsdf_t &desc = s_mtsdf_fonts[ s_num_mtsdf_fonts ];
	std::memset( &desc, 0, sizeof( desc ) );
	Q_strlcpy( desc.path, path, sizeof( desc.path ) );
	desc.pixel_height = pixel_height;
	desc.sdf_pixel_range = MTSDF_PIXEL_RANGE;

	// Compute scale factor for requested pixel height.
	const float scale = stbtt_ScaleForPixelHeight( &font_info, pixel_height );
	int32_t i_ascent = 0, i_descent = 0, i_line_gap = 0;
	stbtt_GetFontVMetrics( &font_info, &i_ascent, &i_descent, &i_line_gap );
	desc.ascent = (float)i_ascent * scale;
	desc.descent = (float)i_descent * scale;
	desc.line_gap = (float)i_line_gap * scale;

	/**
	*	Allocate temporary glyph bitmap cache and packing rectangle list.
	**/
	struct temp_glyph_t {
		uint8_t *bitmap;
		int32_t w, h;
		float bearing_x, bearing_y;
		float advance;
	};
	temp_glyph_t temp_glyphs[ MTSDF_MAX_GLYPHS ];
	std::memset( temp_glyphs, 0, sizeof( temp_glyphs ) );

	stbrp_rect pack_rects[ MTSDF_MAX_GLYPHS ];
	int32_t num_pack_rects = 0;

	/**
	*	Generate multi-channel distance field bitmaps for printable ASCII glyphs.
	**/
	for ( int32_t c = 32; c < 127; c++ ) {
		const int32_t glyph_idx = stbtt_FindGlyphIndex( &font_info, c );
		int32_t advance_w = 0, lsb = 0;
		stbtt_GetCodepointHMetrics( &font_info, c, &advance_w, &lsb );
		temp_glyphs[ c ].advance = (float)advance_w * scale;

		// Handle whitespace and empty glyphs.
		if ( c == ' ' || glyph_idx == 0 ) {
			desc.glyphs[ c ].char_code = c;
			desc.glyphs[ c ].advance = temp_glyphs[ c ].advance;
			desc.glyph_valid[ c ] = true;
			continue;
		}

		// Rasterize individual glyph MTSDF bitmap.
		int32_t gw = 0, gh = 0;
		float bx = 0.0f, by = 0.0f;
		uint8_t *bmp = GenerateGlyphMTSDF( &font_info, glyph_idx, scale, desc.sdf_pixel_range, &gw, &gh, &bx, &by );
		if ( bmp != nullptr ) {
			temp_glyphs[ c ].bitmap = bmp;
			temp_glyphs[ c ].w = gw;
			temp_glyphs[ c ].h = gh;
			temp_glyphs[ c ].bearing_x = bx;
			temp_glyphs[ c ].bearing_y = by;

			pack_rects[ num_pack_rects ].id = c;
			pack_rects[ num_pack_rects ].w = (stbrp_coord)( gw + 2 ); // 1px border padding
			pack_rects[ num_pack_rects ].h = (stbrp_coord)( gh + 2 );
			num_pack_rects++;
		}
	}

	/**
	*	Pack glyph rectangles into an optimal atlas texture size.
	**/
	int32_t atlas_w = 512;
	int32_t atlas_h = 512;
	bool packed = false;

	// Progressively double atlas dimensions if packing fails, up to 2048x2048.
	while ( !packed && atlas_w <= 2048 ) {
		const int32_t num_nodes = atlas_w;
		std::vector< stbrp_node > nodes( num_nodes );
		stbrp_context pack_ctx;
		stbrp_init_target( &pack_ctx, atlas_w, atlas_h, nodes.data(), num_nodes );

		const int32_t all_packed = stbrp_pack_rects( &pack_ctx, pack_rects, num_pack_rects );
		if ( all_packed != 0 ) {
			packed = true;
		} else {
			atlas_w *= 2;
			atlas_h *= 2;
		}
	}

	// Clean up if packing failed.
	if ( !packed ) {
		Com_WPrintf( "%s: Failed to pack font glyphs into atlas\n", __func__ );
		for ( int32_t c = 32; c < 127; c++ ) {
			if ( temp_glyphs[ c ].bitmap != nullptr ) {
				Z_Free( temp_glyphs[ c ].bitmap );
			}
		}
		FS_FreeFile( font_buffer );
		return 0;
	}

	/**
	*	Consolidate glyph pixels into the unified atlas texture buffer.
	**/
	desc.atlas_width = atlas_w;
	desc.atlas_height = atlas_h;
	const size_t atlas_size = (size_t)atlas_w * (size_t)atlas_h * 4;
	uint8_t *atlas_pixels = (uint8_t *)Z_Malloc( (int32_t)atlas_size );
	std::memset( atlas_pixels, 0, atlas_size );

	// Copy each glyph bitmap into its packed atlas location.
	for ( int32_t i = 0; i < num_pack_rects; i++ ) {
		const stbrp_rect &r = pack_rects[ i ];
		const int32_t c = r.id;
		const temp_glyph_t &tg = temp_glyphs[ c ];

		// Copy glyph pixels into packed atlas.
		const int32_t dst_x = r.x + 1;
		const int32_t dst_y = r.y + 1;

		for ( int32_t gy = 0; gy < tg.h; gy++ ) {
			const uint8_t *src_row = tg.bitmap + ( gy * tg.w * 4 );
			uint8_t *dst_row = atlas_pixels + ( ( ( dst_y + gy ) * atlas_w + dst_x ) * 4 );
			std::memcpy( dst_row, src_row, tg.w * 4 );
		}

		// Fill glyph descriptor metrics and UV coordinates.
		font_glyph_mtsdf_t &g = desc.glyphs[ c ];
		g.char_code = c;
		g.advance = tg.advance;
		g.bearing_x = tg.bearing_x;
		g.bearing_y = tg.bearing_y;
		g.width = (float)tg.w;
		g.height = (float)tg.h;
		g.s0 = (float)dst_x / (float)atlas_w;
		g.t0 = (float)dst_y / (float)atlas_h;
		g.s1 = (float)( dst_x + tg.w ) / (float)atlas_w;
		g.t1 = (float)( dst_y + tg.h ) / (float)atlas_h;
		desc.glyph_valid[ c ] = true;

		Z_Free( tg.bitmap );
	}

	/**
	*	Register consolidated atlas texture into engine image subsystem.
	**/
	char atlas_name[ MAX_QPATH ];
	Q_snprintf( atlas_name, sizeof( atlas_name ), "**%s_%.0f**", ( (char *)path ), pixel_height );
	desc.atlas_image = R_RegisterRawImage( atlas_name, atlas_w, atlas_h, atlas_pixels, IT_FONT, (imageflags_t)( IF_PERMANENT | IF_SDF_SILHOUETTE ) );

	// Clean up resources.
	Z_Free( atlas_pixels );
	FS_FreeFile( font_buffer );

	// Commit newly registered font to font registry.
	s_num_mtsdf_fonts++;
	return desc.atlas_image;
}

/**
*	@brief	Query the MTSDF font descriptor associated with a font handle.
*	@param	font	Font handle returned by R_RegisterFontTTF.
*	@return	Pointer to the font_mtsdf_t descriptor, or nullptr if not found.
**/
const font_mtsdf_t *Font_GetDescriptorTTF( const qhandle_t font ) {
	/**
	*	Sanity checks: validate font handle.
	**/
	// Return nullptr for invalid or empty font handles.
	if ( font <= 0 ) {
		return nullptr;
	}

	/**
	*	Search actively loaded font cache for matching atlas image handle.
	**/
	// Iterate through registered MTSDF fonts.
	for ( int32_t i = 0; i < s_num_mtsdf_fonts; i++ ) {
		// Check if atlas image handle matches.
		if ( s_mtsdf_fonts[ i ].atlas_image == font ) {
			return &s_mtsdf_fonts[ i ];
		}
	}

	// Font handle not found in MTSDF cache.
	return nullptr;
}

/**
*	@brief	Measure string width in pixels using TrueType metrics if available.
*	@param	font	Font handle to query.
*	@param	text	Null-terminated text string to measure.
*	@return	Total measured pixel width.
**/
float Font_StringWidthTTF( const qhandle_t font, const char *text ) {
	/**
	*	Sanity checks: return zero width for null or empty strings.
	**/
	// Check if text pointer is null or points to an empty string.
	if ( text == nullptr || text[ 0 ] == '\0' ) {
		return 0.0f;
	}

	/**
	*	Retrieve font descriptor or fall back to legacy fixed-width metrics.
	**/
	const font_mtsdf_t *desc = Font_GetDescriptorTTF( font );
	// If font is legacy bitmap, calculate width using fixed CHAR_WIDTH.
	if ( desc == nullptr ) {
		return (float)( strlen( text ) * CHAR_WIDTH );
	}

	/**
	*	Accumulate glyph advance widths along the string.
	**/
	float total_w = 0.0f;
	// Process each character in sequence.
	while ( *text != '\0' ) {
		const uint8_t c = (uint8_t)( *text++ );
		// Check if character is within valid glyph table range.
		if ( c < MTSDF_MAX_GLYPHS && desc->glyph_valid[ c ] ) {
			total_w += desc->glyphs[ c ].advance;
		} else {
			total_w += desc->glyphs[ ' ' ].advance;
		}
	}

	return total_w;
}

/**
*	@brief	Query the line height of an MTSDF font in pixels.
*	@param	font	Font handle to query.
*	@return	Total line height in pixels (ascent - descent + line_gap).
**/
float Font_GetHeightTTF( const qhandle_t font ) {
	/**
	*	Retrieve font descriptor or fall back to legacy line height.
	**/
	const font_mtsdf_t *desc = Font_GetDescriptorTTF( font );
	// If font is legacy bitmap, return standard CHAR_HEIGHT.
	if ( desc == nullptr ) {
		return (float)CHAR_HEIGHT;
	}
	// Return composite line height.
	return ( desc->ascent - desc->descent + desc->line_gap );
}

