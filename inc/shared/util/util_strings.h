/********************************************************************
*
*
*   String Utilities:
*
*
********************************************************************/
#pragma once

// Stringify macro.
#ifndef STRINGIFY2
#define STRINGIFY2(x)   #x
#endif
#ifndef STRINGIFY
#define STRINGIFY(x)    STRINGIFY2(x)
#endif

// fast "C" macros
//! Checks if a character is uppercase.
#define Q_isupper( c )    ( ( c ) >= 'A' && ( c ) <= 'Z' )
//! Checks if a character is lowercase.
#define Q_islower( c )    ( ( c ) >= 'a' && ( c ) <= 'z' )
//! Checks if a character is a digit.
#define Q_isdigit( c )    ( ( c ) >= '0' && ( c ) <= '9' )
//! Checks if a character is a numeric.
#define Q_isnumeric( c )	( ( c ) == '-' || ( c ) == '+' ) || ( ( c ) >= '0' && ( c ) <= '9' )
//! Checks if a character is one of the float string representable characters.
#define Q_isfloat( c )	( ( c ) == '-' || ( c ) == '+' || ( c ) == '.' ) || ( (c) >= '0' && (c) <= '9') )
//! Checks if the character is alphabetic (a-z, A-Z).
#define Q_isalpha( c )    ( Q_isupper( c ) || Q_islower( c ) )
//! Checks if the character is alphanumeric (a-z, A-Z, 0-9).
#define Q_isalnum( c )    ( Q_isalpha( c ) || Q_isdigit( c ) )
//! Checks if the character is a printable ASCII character (32-126).
#define Q_isprint( c )    ( ( c ) >= 32 && ( c ) < 127 )
//! Checks if the character is a control character (0-31, 127).
#define Q_isgraph( c )    ( ( c ) > 32 && ( c ) < 127 )
//! Checks if the character is a whitespace character (space, form feed, newline, carriage return, tab, vertical tab).
#define Q_isspace( c )    ( c == ' ' || c == '\f' || c == '\n' || \
                         c == '\r' || c == '\t' || c == '\v' )

// tests if specified character is valid quake path character
#define Q_ispath( c )     ( Q_isalnum( c ) || ( c ) == '_' || ( c ) == '-' )

// tests if specified character has special meaning to quake console
#define Q_isspecial( c )  ( ( c ) == '\r' || ( c ) == '\n' || ( c ) == 127 )

// Open extern "C"
QEXTERN_C_OPEN

/**
*	@brief	Converts an uppercase character to its lowercase equivalent.
*	@param	c The character to convert, represented as an integer.
*	@return	The lowercase equivalent of the input character if it is uppercase; otherwise, the input character unchanged.
**/
static inline int Q_tolower( int c ) {
	if ( Q_isupper( c ) ) {
		c += ( 'a' - 'A' );
	}
	return c;
}
/**
*	@brief	Converts a lowercase character to its uppercase equivalent.
*	@param	c The character to convert, represented as an integer.
*	@return	The uppercase equivalent of the input character if it is lowercase; otherwise, the input character unchanged.
&*/
static inline int Q_toupper( int c ) {
	if ( Q_islower( c ) ) {
		c -= ( 'a' - 'A' );
	}
	return c;
}
/**
*	@brief	Converts a string to lowercase.
* 	@param	s The string to convert.
*	@return	A pointer to the converted string.
**/
static inline char *Q_strlwr( char *s ) {
	char *p = s;

	while ( *p ) {
		*p = Q_tolower( *p );
		p++;
	}

	return s;
}
/**
*	@brief	Converts a string to uppercase.
*	@param	s The string to convert.
*	@return	A pointer to the converted string.
**/
static inline char *Q_strupr( char *s ) {
	char *p = s;

	while ( *p ) {
		*p = Q_toupper( *p );
		p++;
	}

	return s;
}
/**
*	@brief	Converts a hexadecimal character to its integer value.
*	@param	c The character to convert, represented as an integer.
*	@return	The integer value of the hexadecimal character if it is valid; otherwise, -1.
**/
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
/**
* @brief	Converts a 'Quake char' to its ASCII equivalent.
* @param	c The character to convert, represented as an integer.
* @return	The ASCII equivalent of the input character if it is printable; otherwise, a dot ('.').
* */
// converts quake char to ASCII equivalent
static inline int Q_charascii( int c ) {
	// White-space chars are output as-is.
	if ( Q_isspace( c ) ) {
		return c;
	}
	// Strip the high bits.
	c &= 127;
	if ( Q_isprint( c ) ) {
		return c;
	}
	// Handle bold brackets.
	switch ( c ) {
		case 16: return '[';
		case 17: return ']';
	}
	return '.'; // Don't output control chars, etc.
}

/**
*	@brief Performs a case-insensitive comparison of two C-style strings.
*	@param s1 The first null-terminated C-style string to compare.
*	@param s2 The second null-terminated C-style string to compare.
*	@return An integer less than, equal to, or greater than zero if s1 is found to be less than, equal to, or greater than s2, respectively, ignoring case.
**/
int Q_strcasecmp( const char *s1, const char *s2 );
/**
*	@brief	Performs a case-insensitive comparison of up to 'n' characters of two strings.
*	@param	s1 The first null-terminated string to compare.
*	@param	s2 The second null-terminated string to compare.
*	@param	n The maximum number of characters to compare.
*	@return	An integer less than, equal to, or greater than zero if the first 'n' characters of 's1' are found to be less than, equal to, or greater than the corresponding characters of 's2', ignoring case.
**/
int Q_strncasecmp( const char *s1, const char *s2, size_t n );
/**
*	@brief	Performs a case-insensitive search for the first occurrence of a substring within a string.
* 	@param	s1 The string to search within.
*	@param	s2 The substring to search for.
*	@return	A pointer to the first occurrence of 's2' within 's1', or NULL if 's2' is not found.
**/
char *Q_strcasestr( const char *s1, const char *s2 );

//! Case insensitive string comparison macros
#define Q_stricmp   Q_strcasecmp
#define Q_stricmpn  Q_strncasecmp
#define Q_stristr   Q_strcasestr

//! Returns a pointer to the first occurrence of 'c' in 's', otherwise it returns where the pointer would be if 'c' was found.
char *Q_strchrnul( const char *s, int c );
//! Copies 'size' bytes from 'src' to 'dst', stopping when 'c' is found. Returns a pointer to the next byte after 'c' in 'dst', or NULL if 'c' was not found.
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

QEXTERN_C_CLOSE

#ifdef __cplusplus
/**
*	@brief	Formats a size in bytes into a human-readable string.
*	@param	bytes The size in bytes to format.
*
*	@return	A string representing the formatted size, such as "1.5 GB", "200 MB", "500 kB", or "100 bytes".
**/
const std::string Q_Str_FormatSizeString( uint64_t bytes, std::string &appendScaleString );
#endif