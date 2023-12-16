#include "shared/shared.h"

/*
============
COM_SkipPath
============
*/
char *COM_SkipPath( const char *pathname ) {
	char *last;

	Q_assert( pathname );

	last = (char *)pathname;
	while ( *pathname ) {
		if ( *pathname == '/' )
			last = (char *)pathname + 1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
size_t COM_StripExtension( char *out, const char *in, size_t size ) {
	size_t ret = COM_FileExtension( in ) - in;

	if ( size ) {
		size_t len = min( ret, size - 1 );
		memcpy( out, in, len );
		out[ len ] = 0;
	}

	return ret;
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension( const char *in ) {
	const char *last, *s;

	Q_assert( in );

	for ( last = s = in + strlen( in ); s != in; s-- ) {
		if ( *s == '/' ) {
			break;
		}
		if ( *s == '.' ) {
			return (char *)s;
		}
	}

	return (char *)last;
}

/*
============
COM_FilePath

Returns the path up to, but not including the last /
============
*/
void COM_FilePath( const char *in, char *out, size_t size ) {
	char *s;

	Q_strlcpy( out, in, size );
	s = strrchr( out, '/' );
	if ( s ) {
		*s = 0;
	} else {
		*out = 0;
	}
}

/*
==================
COM_DefaultExtension

if path doesn't have .EXT, append extension
(extension should include the .)
==================
*/
size_t COM_DefaultExtension( char *path, const char *ext, size_t size ) {
	if ( *COM_FileExtension( path ) )
		return strlen( path );
	else
		return Q_strlcat( path, ext, size );
}

/*
==================
COM_IsFloat

Returns true if the given string is valid representation
of floating point number.
==================
*/
bool COM_IsFloat( const char *s ) {
	int c, dot = '.';

	if ( *s == '-' ) {
		s++;
	}
	if ( !*s ) {
		return false;
	}

	do {
		c = *s++;
		if ( c == dot ) {
			dot = 0;
		} else if ( !Q_isdigit( c ) ) {
			return false;
		}
	} while ( *s );

	return true;
}

bool COM_IsUint( const char *s ) {
	int c;

	if ( !*s ) {
		return false;
	}

	do {
		c = *s++;
		if ( !Q_isdigit( c ) ) {
			return false;
		}
	} while ( *s );

	return true;
}

bool COM_IsPath( const char *s ) {
	int c;

	if ( !*s ) {
		return false;
	}

	do {
		c = *s++;
		if ( !Q_ispath( c ) ) {
			return false;
		}
	} while ( *s );

	return true;
}

bool COM_IsWhite( const char *s ) {
	int c;

	while ( *s ) {
		c = *s++;
		if ( Q_isgraph( c ) ) {
			return false;
		}
	}

	return true;
}

int SortStrcmp( const void *p1, const void *p2 ) {
	return strcmp( *(const char **)p1, *(const char **)p2 );
}

int SortStricmp( const void *p1, const void *p2 ) {
	return Q_stricmp( *(const char **)p1, *(const char **)p2 );
}

/*
================
COM_strclr

Operates inplace, normalizing high-bit and removing unprintable characters.
Returns final number of characters, not including the NUL character.
================
*/
size_t COM_strclr( char *s ) {
	char *p;
	int c;
	size_t len;

	p = s;
	len = 0;
	while ( *s ) {
		c = *s++;
		c &= 127;
		if ( Q_isprint( c ) ) {
			*p++ = c;
			len++;
		}
	}

	*p = 0;

	return len;
}

char *COM_StripQuotes( char *s ) {
	if ( *s == '"' ) {
		size_t p = strlen( s ) - 1;

		if ( s[ p ] == '"' ) {
			s[ p ] = 0;
			return s + 1;
		}
	}

	return s;
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
============
*/
char *va( const char *format, ... ) {
	va_list         argptr;
	static char     buffers[ 4 ][ MAX_STRING_CHARS ];
	static int      index;

	index = ( index + 1 ) & 3;

	va_start( argptr, format );
	Q_vsnprintf( buffers[ index ], sizeof( buffers[ 0 ] ), format, argptr );
	va_end( argptr );

	return buffers[ index ];
}

/*
=============
vtos

This is just a convenience function for printing vectors.
=============
*/
char *vtos( const vec3_t v ) {
	static char str[ 8 ][ 32 ];
	static int  index;

	index = ( index + 1 ) & 7;

	Q_snprintf( str[ index ], sizeof( str[ 0 ] ), "(%.f %.f %.f)", v[ 0 ], v[ 1 ], v[ 2 ] );

	return str[ index ];
}

static char     com_token[ 4 ][ MAX_TOKEN_CHARS ];
static int      com_tokidx;

/*
==============
COM_Parse

Parse a token out of a string.
Handles C and C++ comments.
==============
*/
char *COM_Parse( const char **data_p ) {
	int         c;
	int         len;
	const char *data;
	char *s = com_token[ com_tokidx ];

	com_tokidx = ( com_tokidx + 1 ) & 3;

	data = *data_p;
	len = 0;
	s[ 0 ] = 0;

	if ( !data ) {
		*data_p = NULL;
		return s;
	}

// skip whitespace
skipwhite:
	while ( ( c = *data ) <= ' ' ) {
		if ( c == 0 ) {
			*data_p = NULL;
			return s;
		}
		data++;
	}

// skip // comments
	if ( c == '/' && data[ 1 ] == '/' ) {
		data += 2;
		while ( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}

// skip /* */ comments
	if ( c == '/' && data[ 1 ] == '*' ) {
		data += 2;
		while ( *data ) {
			if ( data[ 0 ] == '*' && data[ 1 ] == '/' ) {
				data += 2;
				break;
			}
			data++;
		}
		goto skipwhite;
	}

// handle quoted strings specially
	if ( c == '\"' ) {
		data++;
		while ( 1 ) {
			c = *data++;
			if ( c == '\"' || !c ) {
				goto finish;
			}

			if ( len < MAX_TOKEN_CHARS - 1 ) {
				s[ len++ ] = c;
			}
		}
	}

// parse a regular word
	do {
		if ( len < MAX_TOKEN_CHARS - 1 ) {
			s[ len++ ] = c;
		}
		data++;
		c = *data;
	} while ( c > 32 );

finish:
	s[ len ] = 0;

	*data_p = data;
	return s;
}

/*
==============
COM_Compress

Operates in place, removing excess whitespace and comments.
Non-contiguous line feeds are preserved.

Returns resulting data length.
==============
*/
size_t COM_Compress( char *data ) {
	int     c, n = 0;
	char *s = data, *d = data;

	while ( *s ) {
		// skip whitespace
		if ( *s <= ' ' ) {
			if ( n == 0 ) {
				n = ' ';
			}
			do {
				c = *s++;
				if ( c == '\n' ) {
					n = '\n';
				}
				if ( !c ) {
					goto finish;
				}
			} while ( *s <= ' ' );
		}

		// skip // comments
		if ( s[ 0 ] == '/' && s[ 1 ] == '/' ) {
			n = ' ';
			s += 2;
			while ( *s && *s != '\n' ) {
				s++;
			}
			continue;
		}

		// skip /* */ comments
		if ( s[ 0 ] == '/' && s[ 1 ] == '*' ) {
			n = ' ';
			s += 2;
			while ( *s ) {
				if ( s[ 0 ] == '*' && s[ 1 ] == '/' ) {
					s += 2;
					break;
				}
				if ( *s == '\n' ) {
					n = '\n';
				}
				s++;
			}
			continue;
		}

		// add whitespace character
		if ( n ) {
			*d++ = n;
			n = 0;
		}

		// handle quoted strings specially
		if ( *s == '\"' ) {
			s++;
			*d++ = '\"';
			do {
				c = *s++;
				if ( !c ) {
					goto finish;
				}
				*d++ = c;
			} while ( c != '\"' );
			continue;
		}

		// handle line feed escape
		if ( *s == '\\' && s[ 1 ] == '\n' ) {
			s += 2;
			continue;
		}
		if ( *s == '\\' && s[ 1 ] == '\r' && s[ 2 ] == '\n' ) {
			s += 3;
			continue;
		}

		// parse a regular word
		do {
			*d++ = *s++;
		} while ( *s > ' ' );
	}

finish:
	*d = 0;

	return d - data;
}

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

int Q_strncasecmp( const char *s1, const char *s2, size_t n ) {
	int        c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( !n-- )
			return 0;        /* strings are equal until end point */

		if ( c1 != c2 ) {
			c1 = Q_tolower( c1 );
			c2 = Q_tolower( c2 );
			if ( c1 < c2 )
				return -1;
			if ( c1 > c2 )
				return 1;        /* strings not equal */
		}
	} while ( c1 );

	return 0;        /* strings are equal */
}

int Q_strcasecmp( const char *s1, const char *s2 ) {
	int        c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( c1 != c2 ) {
			c1 = Q_tolower( c1 );
			c2 = Q_tolower( c2 );
			if ( c1 < c2 )
				return -1;
			if ( c1 > c2 )
				return 1;        /* strings not equal */
		}
	} while ( c1 );

	return 0;        /* strings are equal */
}

char *Q_strcasestr( const char *s1, const char *s2 ) {
	size_t l1, l2;

	l2 = strlen( s2 );
	if ( !l2 ) {
		return (char *)s1;
	}

	l1 = strlen( s1 );
	while ( l1 >= l2 ) {
		l1--;
		if ( !Q_strncasecmp( s1, s2, l2 ) ) {
			return (char *)s1;
		}
		s1++;
	}

	return NULL;
}

/*
===============
Q_strlcpy

Returns length of the source string.
===============
*/
size_t Q_strlcpy( char *dst, const char *src, size_t size ) {
	size_t ret = strlen( src );

	if ( size ) {
		size_t len = min( ret, size - 1 );
		memcpy( dst, src, len );
		dst[ len ] = 0;
	}

	return ret;
}

/*
===============
Q_strlcat

Returns length of the source and destinations strings combined.
===============
*/
size_t Q_strlcat( char *dst, const char *src, size_t size ) {
	size_t len = strlen( dst );

	Q_assert( len < size );

	return len + Q_strlcpy( dst + len, src, size - len );
}

/*
===============
Q_concat_array

Returns number of characters that would be written into the buffer,
excluding trailing '\0'. If the returned value is equal to or greater than
buffer size, resulting string is truncated.
===============
*/
size_t Q_concat_array( char *dest, size_t size, const char **arr ) {
	size_t total = 0;

	while ( *arr ) {
		const char *s = *arr++;
		size_t len = strlen( s );
		if ( total < size ) {
			size_t l = min( size - total - 1, len );
			memcpy( dest, s, l );
			dest += l;
		}
		total += len;
	}

	if ( size ) {
		*dest = 0;
	}

	return total;
}

/*
===============
Q_vsnprintf

Returns number of characters that would be written into the buffer,
excluding trailing '\0'. If the returned value is equal to or greater than
buffer size, resulting string is truncated.
===============
*/
size_t Q_vsnprintf( char *dest, size_t size, const char *fmt, va_list argptr ) {
	int ret;

	Q_assert( size <= INT_MAX );
	ret = vsnprintf( dest, size, fmt, argptr );
	Q_assert( ret >= 0 );

	return ret;
}

/*
===============
Q_vscnprintf

Returns number of characters actually written into the buffer,
excluding trailing '\0'. If buffer size is 0, this function does nothing
and returns 0.
===============
*/
size_t Q_vscnprintf( char *dest, size_t size, const char *fmt, va_list argptr ) {
	if ( size ) {
		size_t ret = Q_vsnprintf( dest, size, fmt, argptr );
		return min( ret, size - 1 );
	}

	return 0;
}

/*
===============
Q_snprintf

Returns number of characters that would be written into the buffer,
excluding trailing '\0'. If the returned value is equal to or greater than
buffer size, resulting string is truncated.
===============
*/
size_t Q_snprintf( char *dest, size_t size, const char *fmt, ... ) {
	va_list argptr;
	size_t  ret;

	va_start( argptr, fmt );
	ret = Q_vsnprintf( dest, size, fmt, argptr );
	va_end( argptr );

	return ret;
}

/*
===============
Q_scnprintf

Returns number of characters actually written into the buffer,
excluding trailing '\0'. If buffer size is 0, this function does nothing
and returns 0.
===============
*/
size_t Q_scnprintf( char *dest, size_t size, const char *fmt, ... ) {
	va_list argptr;
	size_t  ret;

	va_start( argptr, fmt );
	ret = Q_vscnprintf( dest, size, fmt, argptr );
	va_end( argptr );

	return ret;
}

char *Q_strchrnul( const char *s, int c ) {
	while ( *s && *s != c ) {
		s++;
	}
	return (char *)s;
}

/*
===============
Q_memccpy

Copies no more than 'size' bytes stopping when 'c' character is found.
Returns pointer to next byte after 'c' in 'dst', or NULL if 'c' was not found.
===============
*/
void *Q_memccpy( void *dst, const void *src, int c, size_t size ) {
	byte *d = static_cast<byte *>( dst ); // WID: C++20: Added cast.
	const byte *s = static_cast<const byte *>( src ); // WID: C++20: Added cast.

	while ( size-- ) {
		if ( ( *d++ = *s++ ) == c ) {
			return d;
		}
	}

	return NULL;
}

size_t Q_strnlen( const char *s, size_t maxlen ) {
	const char *p = static_cast<const char *>( memchr( s, 0, maxlen ) ); // WID: C++20: Added cast.
	return p ? p - s : maxlen;
}