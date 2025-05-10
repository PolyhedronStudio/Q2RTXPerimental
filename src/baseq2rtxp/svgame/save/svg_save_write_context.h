/********************************************************************
*
*
*	Save Game Mechanism:
*
*	A write context wrapper for conveniently passing around as to 
*	that we can write the game data to/from disk anywhere from
*	anywhere within the code.
*
*
********************************************************************/
#pragma once


//! A write context for the game save/load system.
struct game_write_context_t {
private:
	//! File handle.
	gzFile f;

public:
	//! Create a new write context.
	static game_write_context_t make_write_context( gzFile f );

	//! Write a buffer of data to the file.
	void write_data( const void *buf, size_t len );

	//! Write a short to the file.
	void write_int16( int16_t v );
	//! Write an int32 to the file.
	void write_int32( int32_t v );
	//! Write an int32 to the file.
	void write_int64( int64_t v );

	//! Write a float to the file.
	void write_float( float v );
	//! Write a double to the file.
	void write_double( double v );

	//! Write a string to the file.
	void write_string( const char *s );
	//! Write a game qstring to the file.
	void write_game_qstring( svg_game_qstring_t *qstr );
	//! Write a level qstring to the file.
	void write_level_qstring( svg_level_qstring_t *qstr );
	//! Write a zstring to the file.
	//! Substituted by write_string in the save logic.
	//void write_zstring( const char *s );
	/**
	*   @brief  Write level qtag memory block to disk.
	**/
	template<typename T, int32_t tag = TAG_SVGAME_LEVEL>
	void write_level_qtag_memory( sg_qtag_memory_t<T, tag> *qtagMemory );
	/**
	*   @brief  Write game qtag memory block to disk.
	**/
	template<typename T, int32_t tag = TAG_SVGAME>
	void write_game_qtag_memory( sg_qtag_memory_t<T, tag> *qtagMemory );

	//! Write a vector3 to the file.
	void write_vector3( const vec_t *v );
	//! Write a vector4 to the file.
	void write_vector4( const vec_t *v );

	//! Write a pointer to the file.
	void write_index( const void *p, size_t size, const void *start, int max_index );
	//! Write a pointer to the file.
	void write_pointer( void *p, svg_save_funcptr_type_t type, const svg_save_descriptor_field_t *saveField );

	//! Write a field to the file.
	void write_field( const svg_save_descriptor_field_t *descriptorField, void *base );
	//! Write all fields to the file.
	void write_fields( const svg_save_descriptor_field_t *descriptorFields, void *base );
};