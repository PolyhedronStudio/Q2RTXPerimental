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
#include "refresh/refresh.h"
#include "refresh/images.h"
#include "shared/shared.h"
#include "shared/ui_shared.h"
#include "common/common.h"
#include "common/files.h"
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

//! Magic four-character code for pregenerated MTSDF binary font cache files ('MTSD').
constexpr uint32_t MTSDF_CACHE_MAGIC = 0x4D545344;
//! Version number for pregenerated MTSDF binary font cache file layout.
constexpr uint32_t MTSDF_CACHE_VERSION = 1;

#pragma pack(push, 1)
/**
*	@brief	Binary file header serialized at the beginning of each `.mtsdf` font cache file.
**/
typedef struct font_mtsdf_disk_header_s {
	uint32_t	magic;							//!< 'MTSD' magic identifier
	uint32_t	version;						//!< MTSDF_CACHE_VERSION (1)
	char		path[ MAX_QPATH ];				//!< Original font path
	float		pixel_height;					//!< Nominal raster height
	int32_t		atlas_width;					//!< Atlas texture width in pixels
	int32_t		atlas_height;					//!< Atlas texture height in pixels
	float		ascent;							//!< Font ascent in pixels
	float		descent;						//!< Font descent in pixels
	float		line_gap;						//!< Line gap in pixels
	float		sdf_pixel_range;				//!< Pixel spread range
	uint32_t	pixel_data_size;				//!< atlas_width * atlas_height * 4 bytes
} font_mtsdf_disk_header_t;
#pragma pack(pop)

//! Array of unique TrueType typeface master atlases.
static font_mtsdf_master_t s_master_atlases[ MTSDF_MAX_MASTERS ];
//! Current count of loaded typeface master atlases.
static int32_t s_num_master_atlases = 0;

//! Array of actively instantiated scaled MTSDF fonts.
static font_mtsdf_t s_mtsdf_fonts[ MTSDF_MAX_FONTS ];
//! Current count of actively instantiated MTSDF font instances.
static int32_t s_num_mtsdf_fonts = 0;

//! Color channel bitmasks for MSDF multi-channel signed distance edge coloring.
constexpr int32_t MTSDF_COLOR_BLACK   = 0;
constexpr int32_t MTSDF_COLOR_RED     = 1;
constexpr int32_t MTSDF_COLOR_GREEN   = 2;
constexpr int32_t MTSDF_COLOR_YELLOW  = MTSDF_COLOR_RED | MTSDF_COLOR_GREEN;  // 3
constexpr int32_t MTSDF_COLOR_BLUE    = 4;
constexpr int32_t MTSDF_COLOR_MAGENTA = MTSDF_COLOR_RED | MTSDF_COLOR_BLUE;   // 5
constexpr int32_t MTSDF_COLOR_CYAN    = MTSDF_COLOR_GREEN | MTSDF_COLOR_BLUE; // 6
constexpr int32_t MTSDF_COLOR_WHITE   = MTSDF_COLOR_RED | MTSDF_COLOR_GREEN | MTSDF_COLOR_BLUE; // 7

/**
*	@brief	Representation of an evaluated edge segment for MTSDF distance calculation.
**/
struct mtsdf_edge_t {
	enum edge_type {
		EDGE_LINE,
		EDGE_QUADRATIC
	} type = EDGE_LINE;

	//! Segment start point in font space.
	float p0x = 0.0f;
	float p0y = 0.0f;
	//! Control point for quadratic curves in font space.
	float cx = 0.0f;
	float cy = 0.0f;
	//! Segment end point in font space.
	float p1x = 0.0f;
	float p1y = 0.0f;
	//! Assigned edge color bitmask combination (MTSDF_COLOR_YELLOW, CYAN, MAGENTA, WHITE).
	int32_t color_flags = MTSDF_COLOR_WHITE;
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
	// Sample curve along 16 piecewise linear intervals.
	constexpr int32_t subdivisions = 16;
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
*	@brief	Compute start and end unit tangent vectors for an edge segment.
*	@param	edge		Edge segment.
*	@param	out_t_start	[out] Normalized start tangent vector (X, Y).
*	@param	out_t_end	[out] Normalized end tangent vector (X, Y).
**/
static void ComputeEdgeTangents( const mtsdf_edge_t &edge, float out_t_start[ 2 ], float out_t_end[ 2 ] ) {
	if ( edge.type == mtsdf_edge_t::EDGE_LINE ) {
		float dx = edge.p1x - edge.p0x;
		float dy = edge.p1y - edge.p0y;
		const float len = std::sqrt( ( dx * dx ) + ( dy * dy ) );
		if ( len > 1e-4f ) {
			dx /= len;
			dy /= len;
		} else {
			dx = 0.0f;
			dy = 0.0f;
		}
		out_t_start[ 0 ] = out_t_end[ 0 ] = dx;
		out_t_start[ 1 ] = out_t_end[ 1 ] = dy;
	} else {
		// Quadratic Bézier: start tangent is (cx - p0), end tangent is (p1 - cx).
		float dx0 = edge.cx - edge.p0x;
		float dy0 = edge.cy - edge.p0y;
		float len0 = std::sqrt( ( dx0 * dx0 ) + ( dy0 * dy0 ) );
		if ( len0 <= 1e-4f ) {
			dx0 = edge.p1x - edge.p0x;
			dy0 = edge.p1y - edge.p0y;
			len0 = std::sqrt( ( dx0 * dx0 ) + ( dy0 * dy0 ) );
		}
		if ( len0 > 1e-4f ) {
			dx0 /= len0;
			dy0 /= len0;
		} else {
			dx0 = 0.0f;
			dy0 = 0.0f;
		}
		out_t_start[ 0 ] = dx0;
		out_t_start[ 1 ] = dy0;

		float dx1 = edge.p1x - edge.cx;
		float dy1 = edge.p1y - edge.cy;
		float len1 = std::sqrt( ( dx1 * dx1 ) + ( dy1 * dy1 ) );
		if ( len1 <= 1e-4f ) {
			dx1 = edge.p1x - edge.p0x;
			dy1 = edge.p1y - edge.p0y;
			len1 = std::sqrt( ( dx1 * dx1 ) + ( dy1 * dy1 ) );
		}
		if ( len1 > 1e-4f ) {
			dx1 /= len1;
			dy1 /= len1;
		} else {
			dx1 = 0.0f;
			dy1 = 0.0f;
		}
		out_t_end[ 0 ] = dx1;
		out_t_end[ 1 ] = dy1;
	}
}

/**
*	@brief	Color edge segments of a single closed contour using 3-color MSDF assignment.
*	@param	contour		Vector of edge segments forming a closed contour.
**/
static void ColorContourEdges( std::vector< mtsdf_edge_t > &contour ) {
	const int32_t num_segments = (int32_t)contour.size();
	if ( num_segments <= 0 ) {
		return;
	}

	// Single segment or degenerate contour defaults to WHITE (all channels).
	if ( num_segments == 1 ) {
		contour[ 0 ].color_flags = MTSDF_COLOR_WHITE;
		return;
	}

	// Compute start and end tangents for all segments in contour.
	std::vector< float > t_start( num_segments * 2 );
	std::vector< float > t_end( num_segments * 2 );
	for ( int32_t i = 0; i < num_segments; i++ ) {
		ComputeEdgeTangents( contour[ i ], &t_start[ i * 2 ], &t_end[ i * 2 ] );
	}

	// Identify sharp corner vertices where turn angle >= 35 degrees (cos <= 0.81915).
	constexpr float corner_threshold_cos = 0.81915f;
	std::vector< int32_t > corners;
	corners.reserve( num_segments );

	for ( int32_t i = 0; i < num_segments; i++ ) {
		const int32_t prev_idx = ( i + num_segments - 1 ) % num_segments;
		const float prev_end_x = t_end[ prev_idx * 2 + 0 ];
		const float prev_end_y = t_end[ prev_idx * 2 + 1 ];
		const float cur_start_x = t_start[ i * 2 + 0 ];
		const float cur_start_y = t_start[ i * 2 + 1 ];

		// Dot product of end tangent of previous segment with start tangent of current segment.
		const float dot_val = ( prev_end_x * cur_start_x ) + ( prev_end_y * cur_start_y );
		// Cross product (2D) to detect non-collinear direction change.
		const float cross_val = ( prev_end_x * cur_start_y ) - ( prev_end_y * cur_start_x );

		if ( dot_val <= corner_threshold_cos || ( std::fabs( cross_val ) > 0.5735f ) ) {
			// Vertex i is a sharp corner.
			corners.push_back( i );
		}
	}

	const int32_t num_corners = (int32_t)corners.size();

	// Case 0: Smooth contour with no sharp corners (e.g. circle / 'O').
	if ( num_corners == 0 ) {
		for ( int32_t i = 0; i < num_segments; i++ ) {
			contour[ i ].color_flags = MTSDF_COLOR_WHITE;
		}
		return;
	}

	// Case 1: Exactly 1 sharp corner (e.g. teardrop shape).
	if ( num_corners == 1 ) {
		const int32_t c0 = corners[ 0 ];
		const int32_t half = num_segments / 2;
		for ( int32_t i = 0; i < num_segments; i++ ) {
			const int32_t rel = ( i - c0 + num_segments ) % num_segments;
			contour[ i ].color_flags = ( rel < half ) ? MTSDF_COLOR_YELLOW : MTSDF_COLOR_CYAN;
		}
		return;
	}

	// Case 2: M >= 2 sharp corners. Partition into M parts between corners.
	constexpr int32_t palette[ 3 ] = { MTSDF_COLOR_YELLOW, MTSDF_COLOR_CYAN, MTSDF_COLOR_MAGENTA };
	std::vector< int32_t > part_colors( num_corners );

	for ( int32_t k = 0; k < num_corners; k++ ) {
		part_colors[ k ] = palette[ k % 3 ];
	}

	// Ensure the last part's color does not match the first part's color.
	if ( num_corners > 2 && part_colors[ num_corners - 1 ] == part_colors[ 0 ] ) {
		// Pick the third color that differs from both part_colors[ num_corners - 2 ] and part_colors[ 0 ].
		for ( int32_t p = 0; p < 3; p++ ) {
			if ( palette[ p ] != part_colors[ num_corners - 2 ] && palette[ p ] != part_colors[ 0 ] ) {
				part_colors[ num_corners - 1 ] = palette[ p ];
				break;
			}
		}
	}

	// Assign part colors to segments.
	for ( int32_t k = 0; k < num_corners; k++ ) {
		const int32_t start_seg = corners[ k ];
		const int32_t end_seg = corners[ ( k + 1 ) % num_corners ];
		const int32_t count = ( end_seg >= start_seg ) ? ( end_seg - start_seg ) : ( num_segments - start_seg + end_seg );

		for ( int32_t j = 0; j < count; j++ ) {
			const int32_t seg_idx = ( start_seg + j ) % num_segments;
			contour[ seg_idx ].color_flags = part_colors[ k ];
		}
	}
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
	if ( verts == nullptr || num_verts <= 0 ) {
		return;
	}

	/**
	*	Decompose glyph vertices into individual closed contours.
	**/
	std::vector< std::vector< mtsdf_edge_t > > contours;
	std::vector< mtsdf_edge_t > current_contour;

	float start_x = 0.0f;
	float start_y = 0.0f;
	float cur_x = 0.0f;
	float cur_y = 0.0f;
	bool in_contour = false;

	for ( int32_t i = 0; i < num_verts; i++ ) {
		const stbtt_vertex &v = verts[ i ];

		if ( v.type == STBTT_vmove ) {
			// Close previous contour if open before starting a new one.
			if ( in_contour && !current_contour.empty() ) {
				const float dx = cur_x - start_x;
				const float dy = cur_y - start_y;
				if ( ( dx * dx + dy * dy ) > 1.0f ) {
					mtsdf_edge_t close_edge;
					close_edge.type = mtsdf_edge_t::EDGE_LINE;
					close_edge.p0x = cur_x;
					close_edge.p0y = cur_y;
					close_edge.cx = 0.0f;
					close_edge.cy = 0.0f;
					close_edge.p1x = start_x;
					close_edge.p1y = start_y;
					close_edge.color_flags = MTSDF_COLOR_WHITE;
					current_contour.push_back( close_edge );
				}
				contours.push_back( current_contour );
				current_contour.clear();
			}

			start_x = cur_x = (float)v.x;
			start_y = cur_y = (float)v.y;
			in_contour = true;
		} else if ( v.type == STBTT_vline ) {
			mtsdf_edge_t edge;
			edge.type = mtsdf_edge_t::EDGE_LINE;
			edge.p0x = cur_x;
			edge.p0y = cur_y;
			edge.cx = 0.0f;
			edge.cy = 0.0f;
			edge.p1x = (float)v.x;
			edge.p1y = (float)v.y;
			edge.color_flags = MTSDF_COLOR_WHITE;
			current_contour.push_back( edge );

			cur_x = (float)v.x;
			cur_y = (float)v.y;
		} else if ( v.type == STBTT_vcurve ) {
			mtsdf_edge_t edge;
			edge.type = mtsdf_edge_t::EDGE_QUADRATIC;
			edge.p0x = cur_x;
			edge.p0y = cur_y;
			edge.cx = (float)v.cx;
			edge.cy = (float)v.cy;
			edge.p1x = (float)v.x;
			edge.p1y = (float)v.y;
			edge.color_flags = MTSDF_COLOR_WHITE;
			current_contour.push_back( edge );

			cur_x = (float)v.x;
			cur_y = (float)v.y;
		}
	}

	// Final contour close.
	if ( in_contour && !current_contour.empty() ) {
		const float dx = cur_x - start_x;
		const float dy = cur_y - start_y;
		if ( ( dx * dx + dy * dy ) > 1.0f ) {
			mtsdf_edge_t close_edge;
			close_edge.type = mtsdf_edge_t::EDGE_LINE;
			close_edge.p0x = cur_x;
			close_edge.p0y = cur_y;
			close_edge.cx = 0.0f;
			close_edge.cy = 0.0f;
			close_edge.p1x = start_x;
			close_edge.p1y = start_y;
			close_edge.color_flags = MTSDF_COLOR_WHITE;
			current_contour.push_back( close_edge );
		}
		contours.push_back( current_contour );
	}

	/**
	*	Color each contour using multi-channel edge assignment and collect edges.
	**/
	for ( size_t c = 0; c < contours.size(); c++ ) {
		ColorContourEdges( contours[ c ] );
		for ( size_t e = 0; e < contours[ c ].size(); e++ ) {
			edges.push_back( contours[ c ][ e ] );
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

				// Update channel-specific distances according to active color flags.
				if ( ( edge.color_flags & MTSDF_COLOR_RED ) != 0 && d < min_d_r ) {
					min_d_r = d;
				}
				if ( ( edge.color_flags & MTSDF_COLOR_GREEN ) != 0 && d < min_d_g ) {
					min_d_g = d;
				}
				if ( ( edge.color_flags & MTSDF_COLOR_BLUE ) != 0 && d < min_d_b ) {
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
*	@brief	Serialize an MTSDF font descriptor and its atlas pixel buffer to a binary cache file (.mtsdf).
*	@param	cache_path		Virtual filesystem destination path (e.g. "fonts/segoeui_18.mtsdf").
*	@param	desc			Populated font descriptor containing metrics and glyph data.
*	@param	atlas_pixels	Raw RGBA8 atlas pixel data.
*	@return	True on successful serialization, false on write failure.
**/
/**
*	@brief	Search registered master typeface atlases for a matching font asset path.
*	@param	path	Virtual filesystem path to TrueType or OpenType font file.
*	@return	Index of matching master atlas in s_master_atlases, or -1 if not found.
**/
static int32_t Font_FindMaster( const char *path ) {
	// Loop through currently loaded master typeface atlases.
	for ( int32_t i = 0; i < s_num_master_atlases; i++ ) {
		// Compare font asset paths case-insensitively.
		if ( Q_stricmp( s_master_atlases[ i ].path, path ) == 0 ) {
			return i;
		}
	}
	return -1;
}

/**
*	@brief	Serialize master MTSDF font atlas metrics and pixel buffer to a binary cache file (.mtsdf).
*	@param	cache_path		Virtual filesystem destination path (e.g. "fonts/segoeui.mtsdf").
*	@param	master			Master typeface atlas containing reference metrics.
*	@param	atlas_pixels	Raw RGBA8 atlas pixel data buffer.
*	@return	True on successful write, false on failure.
**/
static bool Font_SaveMasterMTSDF( const char *cache_path, const font_mtsdf_master_t *master, const uint8_t *atlas_pixels ) {
	/**
	*	Sanity checks: validate input parameters.
	**/
	if ( cache_path == nullptr || cache_path[ 0 ] == '\0' || master == nullptr || atlas_pixels == nullptr ) {
		return false;
	}

	/**
	*	Open target file for writing binary payload.
	**/
	qhandle_t f = 0;
	const int64_t ret = FS_OpenFile( cache_path, &f, FS_MODE_WRITE );
	if ( ret < 0 || f == 0 ) {
		Com_WPrintf( "%s: Failed to open '%s' for writing: %s\n", __func__, cache_path, Q_ErrorString( (int32_t)ret ) );
		return false;
	}

	/**
	*	Populate file header fields from the master font atlas descriptor.
	**/
	font_mtsdf_disk_header_t header = {};
	header.magic = MTSDF_CACHE_MAGIC;
	header.version = MTSDF_CACHE_VERSION;
	Q_strlcpy( header.path, master->path, sizeof( header.path ) );
	header.pixel_height = master->ref_pixel_height;
	header.atlas_width = master->atlas_width;
	header.atlas_height = master->atlas_height;
	header.ascent = master->ref_ascent;
	header.descent = master->ref_descent;
	header.line_gap = master->ref_line_gap;
	header.sdf_pixel_range = master->sdf_pixel_range;
	header.pixel_data_size = (uint32_t)( master->atlas_width * master->atlas_height * 4 );

	/**
	*	Write header, reference glyph metric table, valid flags, and RGBA8 atlas pixel bytes.
	**/
	FS_Write( &header, sizeof( header ), f );
	FS_Write( master->ref_glyphs, sizeof( font_glyph_mtsdf_t ) * MTSDF_MAX_GLYPHS, f );
	FS_Write( master->glyph_valid, sizeof( bool ) * MTSDF_MAX_GLYPHS, f );
	FS_Write( atlas_pixels, header.pixel_data_size, f );

	// Close file stream.
	FS_CloseFile( f );

	Com_Printf( "%s: Serialized master MTSDF font cache '%s' (%" PRId32 "x%" PRId32 " at %.0fpx reference height)\n",
		__func__, cache_path, header.atlas_width, header.atlas_height, header.pixel_height );
	return true;
}

/**
*	@brief	Load a precalculated master MTSDF font atlas from a binary cache file (.mtsdf).
*	@param	cache_path		Virtual filesystem path to the cache file (e.g. "fonts/segoeui.mtsdf").
*	@param	original_path	Original font path used for registration and lookup (e.g. "fonts/segoeui.ttf").
*	@return	Index of the loaded master atlas in s_master_atlases, or -1 on failure.
**/
static int32_t Font_LoadMasterMTSDF( const char *cache_path, const char *original_path ) {
	/**
	*	Sanity checks: ensure paths and master atlas capacity are valid.
	**/
	if ( cache_path == nullptr || cache_path[ 0 ] == '\0' || original_path == nullptr || original_path[ 0 ] == '\0' ) {
		return -1;
	}
	if ( s_num_master_atlases >= MTSDF_MAX_MASTERS ) {
		Com_WPrintf( "%s: Maximum MTSDF master atlas capacity reached (%d)\n", __func__, MTSDF_MAX_MASTERS );
		return -1;
	}

	/**
	*	Attempt to load binary cache file from the virtual filesystem.
	**/
	void *cache_buffer = nullptr;
	const int32_t file_size = FS_LoadFileEx( cache_path, &cache_buffer, FS_PATH_ANY, TAG_FILESYSTEM );
	if ( file_size <= 0 || cache_buffer == nullptr ) {
		return -1;
	}

	/**
	*	Validate binary payload size and header signatures.
	**/
	constexpr size_t min_header_size = sizeof( font_mtsdf_disk_header_t )
		+ ( sizeof( font_glyph_mtsdf_t ) * MTSDF_MAX_GLYPHS )
		+ ( sizeof( bool ) * MTSDF_MAX_GLYPHS );

	if ( (size_t)file_size < min_header_size ) {
		Com_WPrintf( "%s: Cache file '%s' is too small (%d bytes)\n", __func__, cache_path, file_size );
		FS_FreeFile( cache_buffer );
		return -1;
	}

	const font_mtsdf_disk_header_t *header = (const font_mtsdf_disk_header_t *)cache_buffer;
	if ( header->magic != MTSDF_CACHE_MAGIC || header->version != MTSDF_CACHE_VERSION ) {
		Com_WPrintf( "%s: Cache file '%s' has invalid magic (0x%08X) or version (%u)\n", __func__, cache_path, header->magic, header->version );
		FS_FreeFile( cache_buffer );
		return -1;
	}

	if ( header->atlas_width <= 0 || header->atlas_height <= 0 || header->pixel_data_size != (uint32_t)( header->atlas_width * header->atlas_height * 4 ) ) {
		Com_WPrintf( "%s: Cache file '%s' contains corrupted atlas dimensions (%dx%d)\n", __func__, cache_path, header->atlas_width, header->atlas_height );
		FS_FreeFile( cache_buffer );
		return -1;
	}

	const size_t expected_total_size = min_header_size + (size_t)header->pixel_data_size;
	if ( (size_t)file_size < expected_total_size ) {
		Com_WPrintf( "%s: Cache file '%s' truncated (expected %zu, got %d)\n", __func__, cache_path, expected_total_size, file_size );
		FS_FreeFile( cache_buffer );
		return -1;
	}

	/**
	*	Allocate texture pixel buffer and extract atlas pixels.
	*	R_RegisterRawImage transfers ownership of the buffer to the renderer.
	**/
	uint8_t *atlas_pixels = (uint8_t *)Z_Malloc( header->pixel_data_size );
	const uint8_t *src_ptr = (const uint8_t *)cache_buffer + sizeof( font_mtsdf_disk_header_t );

	// Populate in-memory master atlas entry.
	const int32_t master_idx = s_num_master_atlases;
	font_mtsdf_master_t &master = s_master_atlases[ master_idx ];
	std::memset( &master, 0, sizeof( master ) );
	Q_strlcpy( master.path, original_path, sizeof( master.path ) );
	master.ref_pixel_height = ( header->pixel_height > 0.0f ) ? header->pixel_height : MTSDF_REFERENCE_HEIGHT;
	master.atlas_width = header->atlas_width;
	master.atlas_height = header->atlas_height;
	master.ref_ascent = header->ascent;
	master.ref_descent = header->descent;
	master.ref_line_gap = header->line_gap;
	master.sdf_pixel_range = header->sdf_pixel_range;

	// Copy reference glyph metrics.
	std::memcpy( master.ref_glyphs, src_ptr, sizeof( font_glyph_mtsdf_t ) * MTSDF_MAX_GLYPHS );
	src_ptr += sizeof( font_glyph_mtsdf_t ) * MTSDF_MAX_GLYPHS;

	// Copy glyph validity flags.
	std::memcpy( master.glyph_valid, src_ptr, sizeof( bool ) * MTSDF_MAX_GLYPHS );
	src_ptr += sizeof( bool ) * MTSDF_MAX_GLYPHS;

	// Copy RGBA8 atlas pixel bytes.
	std::memcpy( atlas_pixels, src_ptr, header->pixel_data_size );

	// Release temporary file buffer.
	FS_FreeFile( cache_buffer );

	/**
	*	Register consolidated atlas texture into engine image subsystem.
	**/
	char atlas_name[ MAX_QPATH ];
	Q_snprintf( atlas_name, sizeof( atlas_name ), "**%s_master**", original_path );
	master.atlas_image = R_RegisterRawImage( atlas_name, master.atlas_width, master.atlas_height, atlas_pixels, IT_FONT, (imageflags_t)( IF_PERMANENT | IF_SDF_SILHOUETTE ) );

	if ( master.atlas_image == 0 ) {
		Z_Free( atlas_pixels );
		return -1;
	}

	// Commit newly loaded master atlas.
	s_num_master_atlases++;
	return master_idx;
}

/**
*	@brief	Generate a master MTSDF font atlas from TrueType vectors at universal reference height.
*	@param	path		Virtual filesystem path to TTF/OTF font asset.
*	@param	cache_path	Virtual filesystem destination path for the generated .mtsdf cache.
*	@return	Index of the created master atlas in s_master_atlases, or -1 on failure.
**/
static int32_t Font_GenerateMasterMTSDF( const char *path, const char *cache_path ) {
	/**
	*	Load TrueType font file binary data from the engine virtual filesystem.
	**/
	void *font_buffer = nullptr;
	const int32_t file_size = FS_LoadFileEx( path, &font_buffer, FS_PATH_ANY, TAG_FILESYSTEM );
	if ( file_size <= 0 || font_buffer == nullptr ) {
		Com_WPrintf( "%s: Failed to load font file '%s'\n", __func__, path );
		return -1;
	}

	/**
	*	Initialize stb_truetype font info structure.
	**/
	const int32_t font_ofs = stbtt_GetFontOffsetForIndex( (const unsigned char *)font_buffer, 0 );
	if ( font_ofs < 0 ) {
		Com_WPrintf( "%s: stbtt_GetFontOffsetForIndex failed for '%s'\n", __func__, path );
		FS_FreeFile( font_buffer );
		return -1;
	}

	stbtt_fontinfo font_info;
	if ( !stbtt_InitFont( &font_info, (const unsigned char *)font_buffer, font_ofs ) ) {
		Com_WPrintf( "%s: stbtt_InitFont failed for '%s'\n", __func__, path );
		FS_FreeFile( font_buffer );
		return -1;
	}

	/**
	*	Initialize master font descriptor metrics at universal reference height (48px).
	**/
	const int32_t master_idx = s_num_master_atlases;
	font_mtsdf_master_t &master = s_master_atlases[ master_idx ];
	std::memset( &master, 0, sizeof( master ) );
	Q_strlcpy( master.path, path, sizeof( master.path ) );
	master.ref_pixel_height = MTSDF_REFERENCE_HEIGHT;
	master.sdf_pixel_range = MTSDF_PIXEL_RANGE;

	// Compute nominal scale factor for reference pixel height.
	const float scale_nominal = stbtt_ScaleForPixelHeight( &font_info, master.ref_pixel_height );
	// At reference height 48px, raster scale is 1:1 since 48px provides ample texel density.
	const float scale_raster = scale_nominal;

	int32_t i_ascent = 0, i_descent = 0, i_line_gap = 0;
	stbtt_GetFontVMetrics( &font_info, &i_ascent, &i_descent, &i_line_gap );
	master.ref_ascent = std::round( (float)i_ascent * scale_nominal );
	master.ref_descent = std::round( (float)i_descent * scale_nominal );
	const float raw_line_gap = (float)i_line_gap * scale_nominal;
	master.ref_line_gap = ( raw_line_gap >= 3.0f ) ? std::round( raw_line_gap ) : std::max( 3.0f, std::round( master.ref_pixel_height * 0.12f ) );

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
		temp_glyphs[ c ].advance = (float)advance_w * scale_nominal;

		// Expand advance width only if glyph geometry strictly overhangs past horizontal advance
		if ( glyph_idx != 0 ) {
			int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
			if ( stbtt_GetGlyphBox( &font_info, glyph_idx, &x0, &y0, &x1, &y1 ) ) {
				const float glyph_right_px = (float)x1 * scale_nominal;
				if ( glyph_right_px > temp_glyphs[ c ].advance ) {
					temp_glyphs[ c ].advance = glyph_right_px + 1.25f;
				}
			}
		}

		// Handle whitespace and empty glyphs.
		if ( c == ' ' || glyph_idx == 0 ) {
			master.ref_glyphs[ c ].char_code = c;
			master.ref_glyphs[ c ].advance = temp_glyphs[ c ].advance;
			master.glyph_valid[ c ] = true;
			continue;
		}

		// Rasterize individual glyph MTSDF bitmap at reference scale.
		int32_t gw = 0, gh = 0;
		float bx = 0.0f, by = 0.0f;
		uint8_t *bmp = GenerateGlyphMTSDF( &font_info, glyph_idx, scale_raster, master.sdf_pixel_range, &gw, &gh, &bx, &by );
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
		return -1;
	}

	/**
	*	Consolidate glyph pixels into the unified atlas texture buffer.
	**/
	master.atlas_width = atlas_w;
	master.atlas_height = atlas_h;
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

		// Fill reference glyph descriptor metrics and UV coordinates.
		font_glyph_mtsdf_t &g = master.ref_glyphs[ c ];
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
		master.glyph_valid[ c ] = true;

		Z_Free( tg.bitmap );
	}

	/**
	*	Automatically serialize generated master MTSDF font data to canonical disk cache file.
	**/
	Font_SaveMasterMTSDF( cache_path, &master, atlas_pixels );

	/**
	*	Register consolidated atlas texture into engine image subsystem.
	**/
	char atlas_name[ MAX_QPATH ];
	Q_snprintf( atlas_name, sizeof( atlas_name ), "**%s_master**", path );
	master.atlas_image = R_RegisterRawImage( atlas_name, atlas_w, atlas_h, atlas_pixels, IT_FONT, (imageflags_t)( IF_PERMANENT | IF_SDF_SILHOUETTE ) );

	// Clean up resources.
	if ( master.atlas_image == 0 ) {
		Z_Free( atlas_pixels );
		FS_FreeFile( font_buffer );
		return -1;
	}
	FS_FreeFile( font_buffer );

	// Commit newly registered master atlas.
	s_num_master_atlases++;
	return master_idx;
}

/**
*	@brief	Locate an existing master atlas or load/generate a new one from disk or TrueType vectors.
*	@param	path	Virtual filesystem path to TrueType font or existing .mtsdf file.
*	@return	Index of master atlas in s_master_atlases, or -1 on failure.
**/
static int32_t Font_FindOrCreateMaster( const char *path ) {
	/**
	*	Sanity checks: ensure valid path.
	**/
	if ( path == nullptr || path[ 0 ] == '\0' ) {
		return -1;
	}

	/**
	*	Check if master atlas for this font path has already been initialized.
	**/
	const int32_t existing_idx = Font_FindMaster( path );
	if ( existing_idx >= 0 ) {
		return existing_idx;
	}

	/**
	*	Check master atlas capacity.
	**/
	if ( s_num_master_atlases >= MTSDF_MAX_MASTERS ) {
		Com_WPrintf( "%s: Maximum MTSDF master atlas capacity reached (%d)\n", __func__, MTSDF_MAX_MASTERS );
		return -1;
	}

	/**
	*	Construct canonical binary cache path (<stem>.mtsdf).
	**/
	char cache_path[ MAX_QPATH ];
	const char *ext = COM_FileExtension( path );
	if ( ext != nullptr && Q_stricmp( ext, ".mtsdf" ) == 0 ) {
		Q_strlcpy( cache_path, path, sizeof( cache_path ) );
	} else {
		char stem[ MAX_QPATH ];
		COM_StripExtension( stem, path, sizeof( stem ) );
		Q_snprintf( cache_path, sizeof( cache_path ), "%s.mtsdf", stem );
	}

	/**
	*	Attempt to load precalculated master MTSDF font cache from disk to bypass CPU distance-field calculation.
	**/
	const int32_t loaded_idx = Font_LoadMasterMTSDF( cache_path, path );
	// If precalculated master cache loaded successfully, return its index.
	if ( loaded_idx >= 0 ) {
		Com_DPrintf( "%s: Loaded precalculated master MTSDF font cache '%s' (master index %" PRId32 ")\n", __func__, cache_path, loaded_idx );
		return loaded_idx;
	}

	/**
	*	No precalculated cache found: proceed to load TrueType font file binary data and generate master atlas.
	**/
	return Font_GenerateMasterMTSDF( path, cache_path );
}

/**
*	@brief	Instantiate a scaled font instance referencing a shared master atlas.
*	@param	master_idx		Index of parent master atlas in s_master_atlases.
*	@param	nominal_height	Requested target pixel height.
*	@return	Synthetic font handle, or 0 if instance capacity is exhausted.
**/
static qhandle_t Font_CreateInstance( const int32_t master_idx, const float nominal_height ) {
	/**
	*	Sanity checks: validate master index and instance capacity.
	**/
	if ( master_idx < 0 || master_idx >= s_num_master_atlases ) {
		return 0;
	}
	if ( s_num_mtsdf_fonts >= MTSDF_MAX_FONTS ) {
		Com_WPrintf( "%s: Maximum MTSDF font instance capacity reached (%d)\n", __func__, MTSDF_MAX_FONTS );
		return 0;
	}

	const font_mtsdf_master_t &master = s_master_atlases[ master_idx ];
	const int32_t inst_idx = s_num_mtsdf_fonts;
	font_mtsdf_t &inst = s_mtsdf_fonts[ inst_idx ];
	std::memset( &inst, 0, sizeof( inst ) );

	inst.font_handle = MTSDF_FONT_HANDLE_BASE + inst_idx;
	inst.master_index = master_idx;
	inst.atlas_image = master.atlas_image;
	Q_strlcpy( inst.path, master.path, sizeof( inst.path ) );
	inst.pixel_height = nominal_height;
	inst.atlas_width = master.atlas_width;
	inst.atlas_height = master.atlas_height;
	inst.sdf_pixel_range = master.sdf_pixel_range;

	/**
	*	Calculate metric scale factor relative to reference master height.
	**/
	const float scale = ( master.ref_pixel_height > 0.0f ) ? ( nominal_height / master.ref_pixel_height ) : 1.0f;

	inst.ascent = master.ref_ascent * scale;
	inst.descent = master.ref_descent * scale;
	inst.line_gap = master.ref_line_gap * scale;

	/**
	*	Scale per-glyph quad geometry while preserving normalized atlas UV coordinates.
	**/
	for ( int32_t c = 0; c < MTSDF_MAX_GLYPHS; c++ ) {
		if ( !master.glyph_valid[ c ] ) {
			continue;
		}
		const font_glyph_mtsdf_t &src = master.ref_glyphs[ c ];
		font_glyph_mtsdf_t &dst = inst.glyphs[ c ];

		dst.char_code = src.char_code;
		dst.advance = src.advance * scale;
		dst.bearing_x = src.bearing_x * scale;
		dst.bearing_y = src.bearing_y * scale;
		dst.width = src.width * scale;
		dst.height = src.height * scale;

		// Normalized UV texture coordinates remain identical across all scaled instances
		dst.s0 = src.s0;
		dst.t0 = src.t0;
		dst.s1 = src.s1;
		dst.t1 = src.t1;

		inst.glyph_valid[ c ] = true;
	}

	// Commit newly registered instance
	s_num_mtsdf_fonts++;
	return inst.font_handle;
}

/**
*	@brief	Serialize an MTSDF font descriptor and its atlas pixel buffer to a binary cache file (.mtsdf).
*	@param	cache_path		Virtual filesystem destination path (e.g. "fonts/segoeui.mtsdf").
*	@param	desc			Populated font descriptor containing metrics and glyph data.
*	@param	atlas_pixels	Raw RGBA8 atlas pixel data.
*	@return	True on successful serialization, false on write failure.
**/
bool Font_SaveMTSDF( const char *cache_path, const font_mtsdf_t *desc, const uint8_t *atlas_pixels ) {
	if ( desc == nullptr || desc->master_index < 0 || desc->master_index >= s_num_master_atlases ) {
		return false;
	}
	return Font_SaveMasterMTSDF( cache_path, &s_master_atlases[ desc->master_index ], atlas_pixels );
}

/**
*	@brief	Load a pregenerated MTSDF font from a binary cache file (.mtsdf).
*	@param	cache_path		Virtual filesystem path to the cache file (e.g. "fonts/segoeui.mtsdf").
*	@param	original_path	Original font path used for registration and lookup (e.g. "fonts/segoeui.ttf").
*	@param	pixel_height	Nominal raster height.
*	@return	Image handle to the registered MTSDF font atlas, or 0 on failure.
**/
qhandle_t Font_LoadMTSDF( const char *cache_path, const char *original_path, const float pixel_height ) {
	const int32_t master_idx = Font_FindOrCreateMaster( original_path ? original_path : cache_path );
	if ( master_idx < 0 ) {
		return 0;
	}
	const float nominal_height = ( pixel_height > 0.0f ) ? pixel_height : (float)DEFAULT_FONT_SIZE;
	return Font_CreateInstance( master_idx, nominal_height );
}

/**
*	@brief	Register and generate a 4-channel MTSDF font from a TrueType / OpenType file.
*	@param	path			Relative path to the TrueType / OpenType font asset.
*	@param	pixel_height	Nominal raster height for the font in pixels.
*	@return	Font handle for the scaled instance, or 0 on failure.
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
	*	Ensure valid reference height, falling back to DEFAULT_FONT_SIZE if zero or negative.
	**/
	const float nominal_height = ( pixel_height > 0.0f ) ? pixel_height : (float)DEFAULT_FONT_SIZE;

	/**
	*	Check font instance cache for an existing instance with identical path and pixel height.
	**/
	// Loop through currently loaded MTSDF font descriptors.
	for ( int32_t i = 0; i < s_num_mtsdf_fonts; i++ ) {
		if ( Q_stricmp( s_mtsdf_fonts[ i ].path, path ) == 0 &&
		     std::fabs( s_mtsdf_fonts[ i ].pixel_height - nominal_height ) < 0.5f ) {
			// Found cached matching font instance.
			return s_mtsdf_fonts[ i ].font_handle;
		}
	}

	/**
	*	Locate or construct the shared typeface master atlas.
	**/
	const int32_t master_idx = Font_FindOrCreateMaster( path );
	if ( master_idx < 0 ) {
		return 0;
	}

	/**
	*	Instantiate a zero-cost scaled font instance pointing to the master atlas.
	**/
	return Font_CreateInstance( master_idx, nominal_height );
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
	*	1. Direct synthetic handle index lookup (O(1)).
	**/
	if ( font >= MTSDF_FONT_HANDLE_BASE && font < ( MTSDF_FONT_HANDLE_BASE + s_num_mtsdf_fonts ) ) {
		const int32_t idx = font - MTSDF_FONT_HANDLE_BASE;
		return &s_mtsdf_fonts[ idx ];
	}

	/**
	*	2. Search by font_handle or atlas_image for backward compatibility.
	**/
	// Iterate through registered MTSDF fonts.
	for ( int32_t i = 0; i < s_num_mtsdf_fonts; i++ ) {
		// Check if font handle or atlas image handle matches.
		if ( s_mtsdf_fonts[ i ].font_handle == font || s_mtsdf_fonts[ i ].atlas_image == font ) {
			return &s_mtsdf_fonts[ i ];
		}
	}

	// Font handle not found in MTSDF cache.
	return nullptr;
}

/**
*	@brief	Compute dynamic character advance and kerning spacing additive for font glyphs
*			to account for outward stroke thickness and outer glow spatial expansion.
*	@param	style_flags			Active STYLE_FLAG_* bitmask.
*	@param	stroke_thickness	Array of 4 stroke thicknesses [Top, Right, Bottom, Left].
*	@param	outer_glow_radius	Array of 4 outer glow radii [Top, Right, Bottom, Left].
*	@return	Pixel spacing additive to apply per character.
**/
float R_Font_CalculateEffectSpacing( const uint32_t style_flags, const float stroke_thickness[ 4 ], const float outer_glow_radius[ 4 ] ) {
	/**
	*	Sanity checks: return zero additive if neither outline stroke nor outer glow are active.
	**/
	if ( ( style_flags & ( STYLE_FLAG_OUTLINE | STYLE_FLAG_OUTER_GLOW ) ) == 0 ) {
		return 0.0f;
	}

	/**
	*	Evaluate horizontal outward stroke outline intrusion between characters.
	**/
	float stroke_additive = 0.0f;
	if ( ( style_flags & STYLE_FLAG_OUTLINE ) != 0 && stroke_thickness != nullptr ) {
		const float stroke_r = stroke_thickness[ 1 ];
		const float stroke_l = stroke_thickness[ 3 ];

		// Branch by stroke alignment to determine outward expansion into inter-glyph gap
		if ( ( style_flags & STYLE_FLAG_STROKE_ALIGN_OUTSET ) != 0 ) {
			// Outset stroke expands fully outward on both sides.
			stroke_additive = stroke_r + stroke_l;
		} else if ( ( style_flags & STYLE_FLAG_STROKE_ALIGN_INSET ) != 0 ) {
			// Inset stroke expands inward, zero gap intrusion.
			stroke_additive = 0.0f;
		} else {
			// Centered stroke (default) expands outward by half its thickness on each side.
			stroke_additive = ( stroke_r * 0.5f ) + ( stroke_l * 0.5f );
		}
	}

	/**
	*	Evaluate horizontal outer glow halo intrusion between characters.
	**/
	float glow_additive = 0.0f;
	if ( ( style_flags & STYLE_FLAG_OUTER_GLOW ) != 0 && outer_glow_radius != nullptr ) {
		const float glow_r = ( ( style_flags & STYLE_FLAG_EDGE_RIGHT ) != 0 ) ? outer_glow_radius[ 1 ] : 0.0f;
		const float glow_l = ( ( style_flags & STYLE_FLAG_EDGE_LEFT ) != 0 ) ? outer_glow_radius[ 3 ] : 0.0f;

		// Outer glow decays with distance; effective half-power halo contribution keeps text cohesive.
		glow_additive = ( glow_r * 0.5f ) + ( glow_l * 0.5f );
	}

	/**
	*	Outer envelope: since stroke and glow radiate from the same vector contour,
	*	take the maximum horizontal expansion envelope.
	**/
	return std::max( stroke_additive, glow_additive );
}

/**
*	@brief	Measure string width in pixels with explicit extra character spacing.
*	@param	font			Font handle to query.
*	@param	text			Null-terminated text string to measure.
*	@param	maxlen			Maximum number of characters to measure.
*	@param	extra_spacing	Additional pixel spacing to apply per character advance.
*	@return	Total measured pixel width.
**/
float Font_StringWidthTTF_Ex( const qhandle_t font, const char *text, const size_t maxlen, const float extra_spacing ) {
	/**
	*	Sanity checks: return zero width for null or empty strings or zero character count.
	**/
	// Check if text pointer is null, empty, or zero max length was requested.
	if ( text == nullptr || text[ 0 ] == '\0' || maxlen == 0 ) {
		return 0.0f;
	}

	/**
	*	Retrieve font descriptor or fall back to legacy fixed-width metrics.
	**/
	const font_mtsdf_t *desc = Font_GetDescriptorTTF( font );
	// If font is legacy bitmap, calculate width using fixed CHAR_WIDTH + extra_spacing.
	if ( desc == nullptr ) {
		size_t len = strlen( text );
		if ( len > maxlen ) {
			len = maxlen;
		}
		return (float)( len * ( (float)CHAR_WIDTH + extra_spacing ) );
	}

	/**
	*	Accumulate glyph advance widths plus extra spacing along the string up to maxlen characters.
	**/
	float total_w = 0.0f;
	size_t remaining = maxlen;
	// Process each character in sequence.
	while ( remaining-- && *text != '\0' ) {
		uint8_t c = (uint8_t)( *text++ );

		// Remap legacy high-bit charset and special console control characters.
		if ( c >= 128 && !desc->glyph_valid[ c ] ) {
			c &= 0x7F;
		}
		if ( c == 17 ) {
			c = ']';
		} else if ( c == 16 ) {
			c = '[';
		} else if ( c == 11 ) {
			c = '_';
		}

		// Accumulate horizontal advance with extra effect spacing additive.
		if ( c < MTSDF_MAX_GLYPHS && desc->glyph_valid[ c ] ) {
			total_w += desc->glyphs[ c ].advance + extra_spacing;
		} else {
			total_w += desc->glyphs[ ' ' ].advance + extra_spacing;
		}
	}

	return total_w;
}

/**
*	@brief	Measure string width in pixels up to maxlen characters using TrueType metrics if available.
*	@param	font	Font handle to query.
*	@param	text	Null-terminated text string to measure.
*	@param	maxlen	Maximum number of characters to measure.
*	@return	Total measured pixel width including active renderer stroke and glow spacing additive.
**/
float Font_StringWidthTTF_N( const qhandle_t font, const char *text, const size_t maxlen ) {
	// Retrieve active renderer font effect spacing additive if function pointer is registered.
	const float spacing_additive = ( R_GetFontEffectSpacing != nullptr ) ? R_GetFontEffectSpacing() : 0.0f;
	return Font_StringWidthTTF_Ex( font, text, maxlen, spacing_additive );
}

/**
*	@brief	Measure string width in pixels using TrueType metrics if available.
*	@param	font	Font handle to query.
*	@param	text	Null-terminated text string to measure.
*	@return	Total measured pixel width.
**/
float Font_StringWidthTTF( const qhandle_t font, const char *text ) {
	return Font_StringWidthTTF_N( font, text, SIZE_MAX );
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

