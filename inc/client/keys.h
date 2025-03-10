/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#pragma once

// Extern C
QEXTERN_C_OPEN

typedef bool (*keywaitcb_t)(void *arg, int key);

void    Key_Init(void);

void    Key_Event(unsigned key, bool down, unsigned time);
void    Key_Event2(unsigned key, bool down, unsigned time);
void    Key_CharEvent(int key);

bool        Key_GetOverstrikeMode(void);
void        Key_SetOverstrikeMode(bool overstrike);
keydest_t   Key_GetDest(void);
void        Key_SetDest(keydest_t dest);

const int32_t	Key_IsDown( const int32_t key);
const int32_t	Key_AnyKeyDown(void);
void			Key_ClearStates(void);

const char  *Key_KeynumToString(int keynum);
int     Key_StringToKeynum(const char *str);
void    Key_SetBinding(int keynum, const char *binding);
const char    *Key_GetBinding(const char *binding);
const char    *Key_GetBindingForKey(int keynum);
int     Key_EnumBindings(int key, const char *binding);
void    Key_WriteBindings(qhandle_t f);

void    Key_WaitKey(keywaitcb_t wait, void *arg);

QEXTERN_C_CLOSE
