/********************************************************************
*
*
*   Info Strings:
*
*
********************************************************************/
#pragma once


#define MAX_INFO_KEY        64
#define MAX_INFO_VALUE      64
#define MAX_INFO_STRING     512

char *Info_ValueForKey( const char *s, const char *key );
void    Info_RemoveKey( char *s, const char *key );
bool    Info_SetValueForKey( char *s, const char *key, const char *value );
bool    Info_Validate( const char *s );
size_t  Info_SubValidate( const char *s );
void    Info_NextPair( const char **string, char *key, char *value );
void    Info_Print( const char *infostring );