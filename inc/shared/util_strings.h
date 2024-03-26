/********************************************************************
*
*
*   String Utilities:
*
*
********************************************************************/
#pragma once

// fast "C" macros
#define Q_isupper(c)    ((c) >= 'A' && (c) <= 'Z')
#define Q_islower(c)    ((c) >= 'a' && (c) <= 'z')
#define Q_isdigit(c)    ((c) >= '0' && (c) <= '9')
#define Q_isalpha(c)    (Q_isupper(c) || Q_islower(c))
#define Q_isalnum(c)    (Q_isalpha(c) || Q_isdigit(c))
#define Q_isprint(c)    ((c) >= 32 && (c) < 127)
#define Q_isgraph(c)    ((c) > 32 && (c) < 127)
#define Q_isspace(c)    (c == ' ' || c == '\f' || c == '\n' || \
                         c == '\r' || c == '\t' || c == '\v')

// tests if specified character is valid quake path character
#define Q_ispath(c)     (Q_isalnum(c) || (c) == '_' || (c) == '-')

// tests if specified character has special meaning to quake console
#define Q_isspecial(c)  ((c) == '\r' || (c) == '\n' || (c) == 127)

static inline int Q_tolower( int c ) {
	if ( Q_isupper( c ) ) {
		c += ( 'a' - 'A' );
	}
	return c;
}

static inline int Q_toupper( int c ) {
	if ( Q_islower( c ) ) {
		c -= ( 'a' - 'A' );
	}
	return c;
}

static inline char *Q_strlwr( char *s ) {
	char *p = s;

	while ( *p ) {
		*p = Q_tolower( *p );
		p++;
	}

	return s;
}

static inline char *Q_strupr( char *s ) {
	char *p = s;

	while ( *p ) {
		*p = Q_toupper( *p );
		p++;
	}

	return s;
}

static inline int Q_charhex( int c ) {
	if ( c >= 'A' && c <= 'F' ) {
		return 10 + ( c - 'A' );
	}
	if ( c >= 'a' && c <= 'f' ) {
		return 10 + ( c - 'a' );
	}
	if ( c >= '0' && c <= '9' ) {
		return c - '0';
	}
	return -1;
}

// converts quake char to ASCII equivalent
static inline int Q_charascii( int c ) {
	if ( Q_isspace( c ) ) {
		// white-space chars are output as-is
		return c;
	}
	c &= 127; // strip high bits
	if ( Q_isprint( c ) ) {
		return c;
	}
	switch ( c ) {
		// handle bold brackets
		case 16: return '[';
		case 17: return ']';
	}
	return '.'; // don't output control chars, etc
}

// portable case insensitive compare
int Q_strcasecmp( const char *s1, const char *s2 );
int Q_strncasecmp( const char *s1, const char *s2, size_t n );
char *Q_strcasestr( const char *s1, const char *s2 );

#define Q_stricmp   Q_strcasecmp
#define Q_stricmpn  Q_strncasecmp
#define Q_stristr   Q_strcasestr

char *Q_strchrnul( const char *s, int c );
void *Q_memccpy( void *dst, const void *src, int c, size_t size );
size_t Q_strnlen( const char *s, size_t maxlen );

char *COM_SkipPath( const char *pathname );
size_t COM_StripExtension( char *out, const char *in, size_t size );
void COM_FilePath( const char *in, char *out, size_t size );
size_t COM_DefaultExtension( char *path, const char *ext, size_t size );
char *COM_FileExtension( const char *in );

#define COM_CompareExtension(in, ext) \
    Q_strcasecmp(COM_FileExtension(in), ext)

/**
*	@return	True if the given string is valid representation
*			of floating point number.
**/
bool COM_IsFloat( const char *s );
/**
*	@return	True if the string is a legitimate SIGNED integer. (So plus(+) and minus(-) allowed at s[0].)
**/
bool COM_IsInt( const char *s );
/**
*	@return	True if the string is a legitimate UNSIGNED integer. (So now minus(-) allowed at s[0].)
**/
bool COM_IsUint( const char *s );
bool COM_IsPath( const char *s );
bool COM_IsWhite( const char *s );

char *COM_Parse( const char **data_p );
// data is an in/out parm, returns a parsed out token
size_t COM_Compress( char *data );

int SortStrcmp( const void *p1, const void *p2 );
int SortStricmp( const void *p1, const void *p2 );

size_t COM_strclr( char *s );
char *COM_StripQuotes( char *s );

/*
=================
Com_WildCmpEx

Wildcard compare. Returns true if string matches the pattern, false otherwise.

- 'term' is handled as an additional filter terminator (besides NUL).
- '*' matches any substring, including the empty string, but prefers longest
possible substrings.
- '?' matches any single character except NUL.
- '\\' can be used to escape any character, including itself. any special
characters lose their meaning in this case.

=================
*/
bool Com_WildCmpEx( const char *filter, const char *string,
	int term, bool ignorecase );
#define Com_WildCmp(filter, string) Com_WildCmpEx(filter, string, 0, false)

// buffer safe operations
size_t Q_strlcpy( char *dst, const char *src, size_t size );
size_t Q_strlcat( char *dst, const char *src, size_t size );

// WID: The define replacement is found in shared_cpp.h for C++
#ifndef __cplusplus
#define Q_concat(dest, size, ...) \
    Q_concat_array(dest, size, (const char *[]){__VA_ARGS__, NULL})
size_t Q_concat_array( char *dest, size_t size, const char **arr );
#endif // __cplusplus

size_t Q_vsnprintf( char *dest, size_t size, const char *fmt, va_list argptr );
size_t Q_vscnprintf( char *dest, size_t size, const char *fmt, va_list argptr );
size_t Q_snprintf( char *dest, size_t size, const char *fmt, ... ) q_printf( 3, 4 );
size_t Q_scnprintf( char *dest, size_t size, const char *fmt, ... ) q_printf( 3, 4 );

char *va( const char *format, ... ) q_printf( 1, 2 );
char *vtos( const vec3_t v );
