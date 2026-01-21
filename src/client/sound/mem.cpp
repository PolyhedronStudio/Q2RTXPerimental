/********************************************************************
*
*
*	Client Sound System: WAV File Loading
*
*	Handles loading and parsing of WAV audio files for the sound system.
*	Supports PCM format WAV files with various bit depths and sample rates.
*	Parses RIFF chunks to extract audio format information and sample data.
*
*
********************************************************************/
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

#include "sound.h"
#include "common/intreadwrite.h"



/**
*
*
*
*	WAV Format Definitions:
*
*
*
**/
//! PCM audio format identifier for WAV files.
#define FORMAT_PCM  1

//! RIFF chunk identifier tag.
#define TAG_RIFF    MakeLittleLong( 'R', 'I', 'F', 'F' )
//! WAVE format chunk identifier.
#define TAG_WAVE    MakeLittleLong( 'W', 'A', 'V', 'E' )
//! Format specification chunk identifier.
#define TAG_fmt     MakeLittleLong( 'f', 'm', 't', ' ' )
//! Cue point chunk identifier (for loop points).
#define TAG_cue     MakeLittleLong( 'c', 'u', 'e', ' ' )
//! List chunk identifier (for metadata).
#define TAG_LIST    MakeLittleLong( 'L', 'I', 'S', 'T' )
//! Mark chunk identifier (for sample marks).
#define TAG_mark    MakeLittleLong( 'm', 'a', 'r', 'k' )
//! Audio data chunk identifier.
#define TAG_data    MakeLittleLong( 'd', 'a', 't', 'a' )

//! Current WAV file information being parsed.
wavinfo_t s_info;



/**
*
*
*
*	RIFF Chunk Parsing:
*
*
*
**/
/**
*	@brief	Searches for a specific RIFF chunk in a size buffer.
*	@param	sz			Size buffer containing WAV file data.
*	@param	search		Chunk identifier tag to search for.
*	@return	Length of the found chunk, or 0 if not found.
*	@note	This function scans through RIFF chunks sequentially, reading the
*			chunk ID and length from each 8-byte chunk header. If the desired
*			chunk is found, its length is returned. Otherwise, the read pointer
*			advances past the current chunk (aligned to 2-byte boundary).
**/
static int FindChunk( sizebuf_t *sz, uint32_t search ) {
	// Scan through chunks until we find the one we're looking for.
	while ( sz->readcount + 8 < sz->cursize ) {
		// Read chunk ID and length.
		uint32_t chunk = SZ_ReadInt32( sz );
		uint32_t len   = SZ_ReadInt32( sz );

		// Clamp length to available data.
		len = min( len, sz->cursize - sz->readcount );

		// Check if this is the chunk we're searching for.
		if ( chunk == search ) {
			return len;
		}

		// Skip to next chunk (aligned to 2-byte boundary).
		sz->readcount += ALIGN( len, 2 );
	}

	return 0;
}



/**
*
*
*
*	WAV File Parsing:
*
*
*
**/
/**
*	@brief	Parses WAV file header and extracts audio format information.
*	@param	sz			Size buffer containing WAV file data.
*	@return	True if WAV file is valid and supported, false otherwise.
*	@note	This function validates the RIFF/WAVE header, parses the format chunk
*			to extract sample rate/bit depth/channels, and locates the audio data.
*			It only supports PCM format WAV files.
**/
static bool GetWavinfo( sizebuf_t *sz ) {
    int tag, samples, width, chunk_len, next_chunk;

    tag = SZ_ReadInt32(sz);

	#if 0
    if (tag == MakeLittleLong('O','g','g','S') || !COM_CompareExtension(s_info.name, ".ogg")) {
        sz->readcount = 0;
        return OGG_Load(sz);
    }
	#endif

// find "RIFF" chunk
    if (tag != TAG_RIFF) {
        Com_DPrintf("%s has missing/invalid RIFF chunk\n", s_info.name);
        return false;
    }

    sz->readcount += 4;
    if (SZ_ReadInt32(sz) != TAG_WAVE) {
        Com_DPrintf("%s has missing/invalid WAVE chunk\n", s_info.name);
        return false;
    }

// save position after "WAVE" tag
    next_chunk = sz->readcount;

// find "fmt " chunk
    if (!FindChunk(sz, TAG_fmt)) {
        Com_DPrintf("%s has missing/invalid fmt chunk\n", s_info.name);
        return false;
    }

    s_info.format = SZ_ReadInt16(sz);
    if (s_info.format != FORMAT_PCM) {
        Com_DPrintf("%s has unsupported format\n", s_info.name);
        return false;
    }

    s_info.channels = SZ_ReadInt16(sz);
    if (s_info.channels < 1 || s_info.channels > 2) {
        Com_DPrintf("%s has bad number of channels\n", s_info.name);
        return false;
    }

    s_info.rate = SZ_ReadInt32(sz);
    if (s_info.rate < 8000 || s_info.rate > 48000) {
        Com_DPrintf("%s has bad rate\n", s_info.name);
        return false;
    }

    sz->readcount += 6;
    width = SZ_ReadInt16(sz);
    switch (width) {
    case 8:
        s_info.width = 1;
        break;
    case 16:
        s_info.width = 2;
        break;
    default:
        Com_DPrintf("%s has bad width\n", s_info.name);
        return false;
    }

// find "data" chunk
    sz->readcount = next_chunk;
    chunk_len = FindChunk(sz, TAG_data);
    if (!chunk_len) {
        Com_DPrintf("%s has missing/invalid data chunk\n", s_info.name);
        return false;
    }

// calculate length in samples
    s_info.samples = chunk_len / (s_info.width * s_info.channels);
    if (!s_info.samples) {
        Com_DPrintf("%s has zero length\n", s_info.name);
        return false;
    }

    s_info.data = sz->data + sz->readcount;
    s_info.loopstart = -1;

// find "cue " chunk
    sz->readcount = next_chunk;
    chunk_len = FindChunk(sz, TAG_cue);
    if (!chunk_len) {
        return true;
    }

// save position after "cue " chunk
    next_chunk = sz->readcount + ALIGN(chunk_len, 2);

    sz->readcount += 24;
    samples = SZ_ReadInt32(sz);
    if (samples < 0 || samples >= s_info.samples) {
        Com_DPrintf("%s has bad loop start\n", s_info.name);
        return true;
    }
    s_info.loopstart = samples;

// if the next chunk is a "LIST" chunk, look for a cue length marker
    sz->readcount = next_chunk;
    if (!FindChunk(sz, TAG_LIST)) {
        return true;
    }

    sz->readcount += 20;
    if (SZ_ReadInt32(sz) != TAG_mark) {
        return true;
    }

// this is not a proper parse, but it works with cooledit...
    sz->readcount -= 8;
    samples = SZ_ReadInt32(sz);  // samples in loop
    if (samples < 1 || samples > s_info.samples - s_info.loopstart) {
        Com_DPrintf("%s has bad loop length\n", s_info.name);
        return true;
    }
    s_info.samples = s_info.loopstart + samples;

    return true;
}

/*
==============
S_LoadSound
==============
*/
sfxcache_t *S_LoadSound(sfx_t *s)
{
    sizebuf_t   sz;
    byte        *data;
    sfxcache_t  *sc;
    int         len;
    char        *name;

    if (s->name[0] == '*')
        return NULL;

// see if still in memory
    sc = s->cache;
    if (sc)
        return sc;

// don't retry after error
    if (s->error)
        return NULL;

// load it in
    if (s->truename)
        name = s->truename;
    else
        name = s->name;

    len = FS_LoadFile(name, (void **)&data);
    if (!data) {
        s->error = len;
        return NULL;
    }

    memset(&s_info, 0, sizeof(s_info));
    s_info.name = name;

    SZ_Init(&sz, data, len);
    sz.cursize = len;

    if (!GetWavinfo(&sz)) {
        s->error = Q_ERR_INVALID_FORMAT;
        goto fail;
    }

#if USE_BIG_ENDIAN
    if (s_info.format == FORMAT_PCM && s_info.width == 2) {
        uint16_t *data = (uint16_t *)s_info.data;
        int count = s_info.samples * s_info.channels;

        for (int i = 0; i < count; i++)
            data[i] = LittleShort(data[i]);
    }
#endif

    sc = s_api.upload_sfx(s);

    if (s_info.format != FORMAT_PCM)
        FS_FreeTempMem(s_info.data);

fail:
    FS_FreeFile(data);
    return sc;
}

