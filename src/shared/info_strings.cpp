#include "shared/shared.h"

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey( const char *s, const char *key ) {
	// use 4 buffers so compares work without stomping on each other
	static char value[ 4 ][ MAX_INFO_STRING ];
	static int  valueindex;
	char        pkey[ MAX_INFO_STRING ];
	char *o;

	valueindex = ( valueindex + 1 ) & 3;
	if ( *s == '\\' )
		s++;
	while ( 1 ) {
		o = pkey;
		while ( *s != '\\' ) {
			if ( !*s )
				goto fail;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[ valueindex ];
		while ( *s != '\\' && *s ) {
			*o++ = *s++;
		}
		*o = 0;

		if ( !strcmp( key, pkey ) )
			return value[ valueindex ];

		if ( !*s )
			goto fail;
		s++;
	}

fail:
	o = value[ valueindex ];
	*o = 0;
	return o;
}

/*
==================
Info_RemoveKey
==================
*/
void Info_RemoveKey( char *s, const char *key ) {
	char *start;
	char    pkey[ MAX_INFO_STRING ];
	char *o;

	while ( 1 ) {
		start = s;
		if ( *s == '\\' )
			s++;
		o = pkey;
		while ( *s != '\\' ) {
			if ( !*s )
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		while ( *s != '\\' && *s ) {
			s++;
		}

		if ( !strcmp( key, pkey ) ) {
			o = start; // remove this part
			while ( *s ) {
				*o++ = *s++;
			}
			*o = 0;
			s = start;
			continue; // search for duplicates
		}

		if ( !*s )
			return;
	}

}


/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing.
Also checks the length of keys/values and the whole string.
==================
*/
bool Info_Validate( const char *s ) {
	size_t len, total;
	int c;

	total = 0;
	while ( 1 ) {
		//
		// validate key
		//
		if ( *s == '\\' ) {
			s++;
			if ( ++total == MAX_INFO_STRING ) {
				return false;   // oversize infostring
			}
		}
		if ( !*s ) {
			return false;   // missing key
		}
		len = 0;
		while ( *s != '\\' ) {
			c = *s++;
			if ( !Q_isprint( c ) || c == '\"' || c == ';' ) {
				return false;   // illegal characters
			}
			if ( ++len == MAX_INFO_KEY ) {
				return false;   // oversize key
			}
			if ( ++total == MAX_INFO_STRING ) {
				return false;   // oversize infostring
			}
			if ( !*s ) {
				return false;   // missing value
			}
		}

		//
		// validate value
		//
		s++;
		if ( ++total == MAX_INFO_STRING ) {
			return false;   // oversize infostring
		}
		if ( !*s ) {
			return false;   // missing value
		}
		len = 0;
		while ( *s != '\\' ) {
			c = *s++;
			if ( !Q_isprint( c ) || c == '\"' || c == ';' ) {
				return false;   // illegal characters
			}
			if ( ++len == MAX_INFO_VALUE ) {
				return false;   // oversize value
			}
			if ( ++total == MAX_INFO_STRING ) {
				return false;   // oversize infostring
			}
			if ( !*s ) {
				return true;    // end of string
			}
		}
	}

	return false; // quiet compiler warning
}

/*
============
Info_SubValidate
============
*/
size_t Info_SubValidate( const char *s ) {
	size_t len;
	int c;

	len = 0;
	while ( *s ) {
		c = *s++;
		c &= 127;       // strip high bits
		if ( c == '\\' || c == '\"' || c == ';' ) {
			return SIZE_MAX;  // illegal characters
		}
		if ( ++len == MAX_QPATH ) {
			return MAX_QPATH;  // oversize value
		}
	}

	return len;
}

/*
==================
Info_SetValueForKey
==================
*/
bool Info_SetValueForKey( char *s, const char *key, const char *value ) {
	char    newi[ MAX_INFO_STRING ], *v;
	size_t  l, kl, vl;
	int     c;

	// validate key
	kl = Info_SubValidate( key );
	if ( kl >= MAX_QPATH ) {
		return false;
	}

	// validate value
	vl = Info_SubValidate( value );
	if ( vl >= MAX_QPATH ) {
		return false;
	}

	Info_RemoveKey( s, key );
	if ( !vl ) {
		return true;
	}

	l = strlen( s );
	if ( l + kl + vl + 2 >= MAX_INFO_STRING ) {
		return false;
	}

	newi[ 0 ] = '\\';
	memcpy( newi + 1, key, kl );
	newi[ kl + 1 ] = '\\';
	memcpy( newi + kl + 2, value, vl + 1 );

	// only copy ascii values
	s += l;
	v = newi;
	while ( *v ) {
		c = *v++;
		c &= 127;        // strip high bits
		if ( Q_isprint( c ) )
			*s++ = c;
	}
	*s = 0;

	return true;
}

/*
==================
Info_NextPair
==================
*/
void Info_NextPair( const char **string, char *key, char *value ) {
	char *o;
	const char *s;

	*value = *key = 0;

	s = *string;
	if ( !s ) {
		return;
	}

	if ( *s == '\\' )
		s++;

	if ( !*s ) {
		*string = NULL;
		return;
	}

	o = key;
	while ( *s && *s != '\\' ) {
		*o++ = *s++;
	}
	*o = 0;

	if ( !*s ) {
		*string = NULL;
		return;
	}

	o = value;
	s++;
	while ( *s && *s != '\\' ) {
		*o++ = *s++;
	}
	*o = 0;

	*string = s;
}

/*
==================
Info_Print
==================
*/
void Info_Print( const char *infostring ) {
	char    key[ MAX_INFO_STRING ];
	char    value[ MAX_INFO_STRING ];

	while ( 1 ) {
		Info_NextPair( &infostring, key, value );
		if ( !infostring )
			break;

		if ( !key[ 0 ] )
			strcpy( key, "<MISSING KEY>" );

		if ( !value[ 0 ] )
			strcpy( value, "<MISSING VALUE>" );

		Com_Printf( "%-20s %s\n", key, value );
	}
}
