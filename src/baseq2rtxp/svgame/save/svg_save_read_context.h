/********************************************************************
*
*
*	Save Game Mechanism:
*
*	A read context wrapper for conveniently passing around as to
*	that we can read the game data to/from disk anywhere from
*	anywhere within the code.
*
*
********************************************************************/
#pragma once


/**
*
*
*   Save Game Read Context:
*
*
**/
//! A read context for the game save/load system.
struct game_read_context_t {
private:
	//! File handle.
	gzFile f;

public:
	//! Create a new read context.
	static game_read_context_t make_read_context( gzFile f );

	//!
	//! 
	//! 
	//! 
	//! 
	void read_data( void *buf, size_t len );
	//! 
	const int16_t read_short();
	//!
	const int32_t read_int32();
	//!
	const int64_t read_int64();
	//! 
	const float read_float();
	//! 
	const double read_double();
	//! 
	char *read_string();
	//!
	const svg_level_qstring_t read_level_qstring();
	//!
	const svg_game_qstring_t read_game_qstring();
	//! 
	template<typename T>
	sg_qtag_memory_t<T, TAG_SVGAME_LEVEL> *read_level_qtag_memory( sg_qtag_memory_t<T, TAG_SVGAME_LEVEL> *p );
	//! 
	template<typename T>
	sg_qtag_memory_t<T, TAG_SVGAME> *read_game_qtag_memory( sg_qtag_memory_t<T, TAG_SVGAME> *p );
	//! Read a buffer of data from the file.
	void read_zstring( char *s, size_t size );
	//! Read a vector3
	void read_vector3( vec_t *v );
	//! Read a vector4
	void read_vector4( vec_t *v );
	//! Read an index range from the file.
	void *read_index( size_t size, void *start, int max_index );
	//! Read the function pointer index from file.
	void *read_pointer( svg_save_funcptr_type_t type );

	//! Read a field from the file.
	void read_field( const svg_save_descriptor_field_t *descriptorField, void *base );
	//! Read all fields from the file.
	void read_fields( const svg_save_descriptor_field_t *descriptorFields, void *base );
};
