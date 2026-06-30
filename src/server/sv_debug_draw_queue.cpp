#include "sv_debug_draw_queue.h"

#if USE_CLIENT

#include <mutex>
#include <vector>

/**
*	Constants & Structures
**/
#define MAX_SV_DEBUG_BOXES    ( 131072 * 4 )
#define MAX_SV_DEBUG_LINES    ( 131072 * 4 )
#define MAX_SV_DEBUG_ARROWS   ( 131072 )
#define MAX_SV_DEBUG_SPHERES  ( 131072 )
#define MAX_SV_DEBUG_CAPSULES ( 131072 )
#define MAX_SV_DEBUG_CYLINDERS ( 131072 )

struct sv_debug_box_t {
	vec3_t mins;
	vec3_t maxs;
	uint32_t color;
	float thickness;
	float outline_thickness;
	uint16_t style_flags;
};

struct sv_debug_line_t {
	vec3_t start;
	vec3_t end;
	uint32_t color;
	float thickness;
	float outline_thickness;
	uint16_t style_flags;
};

struct sv_debug_arrow_t {
	vec3_t start;
	vec3_t end;
	float head_length;
	uint32_t color;
	float thickness;
	float outline_thickness;
	uint16_t style_flags;
};

struct sv_debug_sphere_t {
	vec3_t center;
	float radius;
	uint32_t color;
	float thickness;
	float outline_thickness;
	uint16_t style_flags;
};

struct sv_debug_capsule_t {
	vec3_t start;
	vec3_t end;
	float radius;
	uint32_t color;
	float thickness;
	float outline_thickness;
	uint16_t style_flags;
};

struct sv_debug_cylinder_t {
	vec3_t start;
	vec3_t end;
	float radius;
	uint32_t color;
	float thickness;
	float outline_thickness;
	uint16_t style_flags;
};

/**
*	Thread-Safe Global Queues
**/
static std::mutex s_sv_debug_draw_mutex;

static sv_debug_box_t s_sv_debug_boxes[ MAX_SV_DEBUG_BOXES ];
static int32_t s_sv_debug_num_boxes = 0;

static sv_debug_line_t s_sv_debug_lines[ MAX_SV_DEBUG_LINES ];
static int32_t s_sv_debug_num_lines = 0;

static sv_debug_arrow_t s_sv_debug_arrows[ MAX_SV_DEBUG_ARROWS ];
static int32_t s_sv_debug_num_arrows = 0;

static sv_debug_sphere_t s_sv_debug_spheres[ MAX_SV_DEBUG_SPHERES ];
static int32_t s_sv_debug_num_spheres = 0;

static sv_debug_capsule_t s_sv_debug_capsules[ MAX_SV_DEBUG_CAPSULES ];
static int32_t s_sv_debug_num_capsules = 0;

static sv_debug_cylinder_t s_sv_debug_cylinders[ MAX_SV_DEBUG_CYLINDERS ];
static int32_t s_sv_debug_num_cylinders = 0;

/**
*	External Client Imports (Populated via cl_refresh or cl_clientgame)
**/
extern "C" {
	extern void( *R_DrawDebugBox )( const vec3_t mins, const vec3_t maxs, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
	extern void( *R_DrawDebugLine )( const vec3_t start, const vec3_t end, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
	extern void( *R_DrawDebugArrow )( const vec3_t start, const vec3_t end, float head_length, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
	extern void( *R_DrawDebugSphere )( const vec3_t center, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
	extern void( *R_DrawDebugCapsule )( const vec3_t start, const vec3_t end, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
	extern void( *R_DrawDebugCylinder )( const vec3_t start, const vec3_t end, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
};

/**
*	@brief	Initialize or reset the persistent debug draw queues.
**/
void SV_ClearDebugDrawQueues( void ) {
	std::lock_guard<std::mutex> lock( s_sv_debug_draw_mutex );
	s_sv_debug_num_boxes = 0;
	s_sv_debug_num_lines = 0;
	s_sv_debug_num_arrows = 0;
	s_sv_debug_num_spheres = 0;
	s_sv_debug_num_capsules = 0;
	s_sv_debug_num_cylinders = 0;
}

/**
*	@brief	Submit the persistent debug draw queues to the client renderer.
**/
void SV_SubmitDebugDrawQueues( void ) {
	std::lock_guard<std::mutex> lock( s_sv_debug_draw_mutex );

	if ( R_DrawDebugBox ) {
		for ( int32_t i = 0; i < s_sv_debug_num_boxes; i++ ) {
			R_DrawDebugBox( s_sv_debug_boxes[ i ].mins, s_sv_debug_boxes[ i ].maxs, s_sv_debug_boxes[ i ].color, s_sv_debug_boxes[ i ].thickness, s_sv_debug_boxes[ i ].outline_thickness, s_sv_debug_boxes[ i ].style_flags );
		}
	}

	if ( R_DrawDebugLine ) {
		for ( int32_t i = 0; i < s_sv_debug_num_lines; i++ ) {
			R_DrawDebugLine( s_sv_debug_lines[ i ].start, s_sv_debug_lines[ i ].end, s_sv_debug_lines[ i ].color, s_sv_debug_lines[ i ].thickness, s_sv_debug_lines[ i ].outline_thickness, s_sv_debug_lines[ i ].style_flags );
		}
	}

	if ( R_DrawDebugArrow ) {
		for ( int32_t i = 0; i < s_sv_debug_num_arrows; i++ ) {
			R_DrawDebugArrow( s_sv_debug_arrows[ i ].start, s_sv_debug_arrows[ i ].end, s_sv_debug_arrows[ i ].head_length, s_sv_debug_arrows[ i ].color, s_sv_debug_arrows[ i ].thickness, s_sv_debug_arrows[ i ].outline_thickness, s_sv_debug_arrows[ i ].style_flags );
		}
	}

	if ( R_DrawDebugSphere ) {
		for ( int32_t i = 0; i < s_sv_debug_num_spheres; i++ ) {
			R_DrawDebugSphere( s_sv_debug_spheres[ i ].center, s_sv_debug_spheres[ i ].radius, s_sv_debug_spheres[ i ].color, s_sv_debug_spheres[ i ].thickness, s_sv_debug_spheres[ i ].outline_thickness, s_sv_debug_spheres[ i ].style_flags );
		}
	}

	if ( R_DrawDebugCapsule ) {
		for ( int32_t i = 0; i < s_sv_debug_num_capsules; i++ ) {
			R_DrawDebugCapsule( s_sv_debug_capsules[ i ].start, s_sv_debug_capsules[ i ].end, s_sv_debug_capsules[ i ].radius, s_sv_debug_capsules[ i ].color, s_sv_debug_capsules[ i ].thickness, s_sv_debug_capsules[ i ].outline_thickness, s_sv_debug_capsules[ i ].style_flags );
		}
	}

	if ( R_DrawDebugCylinder ) {
		for ( int32_t i = 0; i < s_sv_debug_num_cylinders; i++ ) {
			R_DrawDebugCylinder( s_sv_debug_cylinders[ i ].start, s_sv_debug_cylinders[ i ].end, s_sv_debug_cylinders[ i ].radius, s_sv_debug_cylinders[ i ].color, s_sv_debug_cylinders[ i ].thickness, s_sv_debug_cylinders[ i ].outline_thickness, s_sv_debug_cylinders[ i ].style_flags );
		}
	}
}

/**
*	@brief	Engine-wrapped game import implementations that push primitives
*			into the thread-safe persistent debug queues.
**/
void PF_SV_R_DrawDebugBox( const vec3_t mins, const vec3_t maxs, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags ) {
	std::lock_guard<std::mutex> lock( s_sv_debug_draw_mutex );
	if ( s_sv_debug_num_boxes >= MAX_SV_DEBUG_BOXES ) {
		return;
	}
	sv_debug_box_t *box = &s_sv_debug_boxes[ s_sv_debug_num_boxes++ ];
	VectorCopy( mins, box->mins );
	VectorCopy( maxs, box->maxs );
	box->color = color;
	box->thickness = thickness;
	box->outline_thickness = outline_thickness;
	box->style_flags = style_flags;
}

void PF_SV_R_DrawDebugLine( const vec3_t start, const vec3_t end, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags ) {
	std::lock_guard<std::mutex> lock( s_sv_debug_draw_mutex );
	if ( s_sv_debug_num_lines >= MAX_SV_DEBUG_LINES ) {
		return;
	}
	sv_debug_line_t *line = &s_sv_debug_lines[ s_sv_debug_num_lines++ ];
	VectorCopy( start, line->start );
	VectorCopy( end, line->end );
	line->color = color;
	line->thickness = thickness;
	line->outline_thickness = outline_thickness;
	line->style_flags = style_flags;
}

void PF_SV_R_DrawDebugArrow( const vec3_t start, const vec3_t end, float head_length, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags ) {
	std::lock_guard<std::mutex> lock( s_sv_debug_draw_mutex );
	if ( s_sv_debug_num_arrows >= MAX_SV_DEBUG_ARROWS ) {
		return;
	}
	sv_debug_arrow_t *arrow = &s_sv_debug_arrows[ s_sv_debug_num_arrows++ ];
	VectorCopy( start, arrow->start );
	VectorCopy( end, arrow->end );
	arrow->head_length = head_length;
	arrow->color = color;
	arrow->thickness = thickness;
	arrow->outline_thickness = outline_thickness;
	arrow->style_flags = style_flags;
}

void PF_SV_R_DrawDebugSphere( const vec3_t center, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags ) {
	std::lock_guard<std::mutex> lock( s_sv_debug_draw_mutex );
	if ( s_sv_debug_num_spheres >= MAX_SV_DEBUG_SPHERES ) {
		return;
	}
	sv_debug_sphere_t *sphere = &s_sv_debug_spheres[ s_sv_debug_num_spheres++ ];
	VectorCopy( center, sphere->center );
	sphere->radius = radius;
	sphere->color = color;
	sphere->thickness = thickness;
	sphere->outline_thickness = outline_thickness;
	sphere->style_flags = style_flags;
}

void PF_SV_R_DrawDebugCapsule( const vec3_t start, const vec3_t end, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags ) {
	std::lock_guard<std::mutex> lock( s_sv_debug_draw_mutex );
	if ( s_sv_debug_num_capsules >= MAX_SV_DEBUG_CAPSULES ) {
		return;
	}
	sv_debug_capsule_t *capsule = &s_sv_debug_capsules[ s_sv_debug_num_capsules++ ];
	VectorCopy( start, capsule->start );
	VectorCopy( end, capsule->end );
	capsule->radius = radius;
	capsule->color = color;
	capsule->thickness = thickness;
	capsule->outline_thickness = outline_thickness;
	capsule->style_flags = style_flags;
}

void PF_SV_R_DrawDebugCylinder( const vec3_t start, const vec3_t end, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags ) {
	std::lock_guard<std::mutex> lock( s_sv_debug_draw_mutex );
	if ( s_sv_debug_num_cylinders >= MAX_SV_DEBUG_CYLINDERS ) {
		return;
	}
	sv_debug_cylinder_t *cylinder = &s_sv_debug_cylinders[ s_sv_debug_num_cylinders++ ];
	VectorCopy( start, cylinder->start );
	VectorCopy( end, cylinder->end );
	cylinder->radius = radius;
	cylinder->color = color;
	cylinder->thickness = thickness;
	cylinder->outline_thickness = outline_thickness;
	cylinder->style_flags = style_flags;
}

#endif // USE_CLIENT
