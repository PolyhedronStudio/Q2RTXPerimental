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

/********************************************************************
*
*
*	Module Name: DMA Sound Backend
*
*	This module implements the legacy DMA (Direct Memory Access) sound backend
*	for Q2RTXPerimental. It provides software-based audio mixing and streaming
*	to system audio devices through platform-specific interfaces (Windows DirectSound,
*	Linux ALSA/PulseAudio, etc.).
*
*	Architecture:
*	- Software audio mixing using integer arithmetic
*	- Circular buffer for raw streaming samples (music/cinematics)
*	- Sample rate conversion (resampling) for compatibility
*	- Fixed-point fractional indexing for efficient interpolation
*	- Stereo panning and volume control
*
*	Key Components:
*	- Paint Buffer: Temporary mixing buffer for audio rendering
*	- Raw Sample Ring Buffer: Circular buffer for streaming audio
*	- Resampling Engine: Converts between arbitrary sample rates
*	- DMA Buffer: Platform-specific hardware buffer interface
*
*	Performance Characteristics:
*	- Supports arbitrary sample rates (typically 11025, 22050, 44100 Hz)
*	- 8-bit or 16-bit PCM audio
*	- Mono or stereo output
*	- Software mixing limited by CPU performance
*
*
********************************************************************/

#include "sound.h"
#include "common/intreadwrite.h"

/**
*
*
*
*	Configuration Constants:
*
*
*
**/

#define PAINTBUFFER_SIZE    2048	//! Size of temporary mixing buffer in sample pairs.
#define MAX_RAW_SAMPLES     8192	//! Maximum raw samples in circular buffer for streaming audio.

/**
*
*
*
*	Module State:
*
*
*
**/

//! DMA buffer configuration and state.
dma_t dma;

//! Console variable for sample rate selection (in kHz).
cvar_t *s_khz;

//! Test sound generation for debugging audio pipeline.
static cvar_t *s_testsound;

//! Swap left/right stereo channels.
static cvar_t *s_swapstereo;

//! Mixing buffer lead time in milliseconds.
static cvar_t *s_mixahead;

//! Cached volume multiplier for mixing operations.
static float snd_vol;

//! Current write position in raw sample ring buffer.
static int s_rawend;

//! Circular buffer for raw streaming audio samples (music/cinematics).
static samplepair_t s_rawsamples[MAX_RAW_SAMPLES];

/**
*
*
*
*	Sound Loading and Resampling:
*
*
*
**/

/**
*	Resampling loop macro using fixed-point arithmetic for efficient interpolation.
*	i = output sample index, j = input sample index (integer part), frac = fractional part.
**/
#define RESAMPLE \
	for ( i = frac = 0; j = frac >> 8, i < outcount; i++, frac += fracstep )

/**
*	@brief	Upload and resample sound effect data to match DMA output rate.
*	@param	sfx	Sound effect structure containing metadata and raw audio data.
*	@return	Pointer to allocated sfxcache structure, or NULL on error.
*	@note	Performs sample rate conversion using fixed-point fractional indexing.
*	@note	Optimized fast path for 1:1 resampling (no rate conversion needed).
*	@note	Supports mono/stereo, 8-bit/16-bit PCM formats.
**/
static sfxcache_t *DMA_UploadSfx( sfx_t *sfx ) {
	float stepscale = ( float )s_info.rate / dma.speed;   // Resampling ratio (typically 0.5, 1.0, or 2.0).
	int i, j, frac, fracstep = stepscale * 256;

	// Calculate output sample count after resampling.
	int outcount = s_info.samples / stepscale;
	if ( !outcount ) {
		Com_DPrintf( "%s resampled to zero length\n", s_info.name );
		sfx->error = Q_ERR_TOO_FEW;
		return nullptr;
	}

	// Allocate sfxcache structure with embedded sample data.
	int size = outcount * s_info.width * s_info.channels;
	sfxcache_t *sc = sfx->cache = S_Malloc( sizeof( sfxcache_t ) + size - 1 );

	sc->length = outcount;
	sc->loopstart = s_info.loopstart == -1 ? -1 : s_info.loopstart / stepscale;
	sc->width = s_info.width;
	sc->channels = s_info.channels;
	sc->size = size;

	// Resample/decimate to the current output sample rate.
	if ( stepscale == 1 ) {
		// Fast path: No resampling needed, direct copy.
		memcpy( sc->data, s_info.data, size );
	} else if ( sc->width == 1 && sc->channels == 1 ) {
		// 8-bit mono resampling.
		RESAMPLE sc->data[i] = s_info.data[j];
	} else if ( sc->width == 2 && sc->channels == 2 ) {
		// 16-bit stereo resampling (optimized for 4-byte pairs).
		RESAMPLE WL32( sc->data + i * 4, RL32( s_info.data + j * 4 ) );
	} else {
		// 16-bit mono resampling.
		RESAMPLE ( ( uint16_t * )sc->data )[i] = ( ( uint16_t * )s_info.data )[j];
	}

	return sc;
}

/**
*	@brief	Stub function for EAX effect properties (DMA backend does not support EAX).
*	@param	eax_properties	Reverb properties structure (ignored).
*	@return	Always returns false (EAX not supported).
**/
static const qboolean DMA_SetEAXEFfectProperties( const sfx_eax_properties_t *eax_properties ) {
	return false;
}

#undef RESAMPLE

/**
*	@brief	Page sound effect data into physical memory to prevent paging delays.
*	@param	sfx	Sound effect structure containing cached sample data.
*	@note	Touches all pages of sound data to ensure they are resident in RAM.
**/
static void DMA_PageInSfx( sfx_t *sfx ) {
	sfxcache_t *sc = sfx->cache;
	if ( sc )
		Com_PageInMemory( sc->data, sc->size );
}

/**
*
*
*
*	Raw Sample Streaming (Music/Cinematics):
*
*
*
**/

/**
*	Resampling loop macro for raw samples using circular buffer indexing.
*	i = output index, j = ring buffer index, k = input sample index, frac = fractional part.
**/
#define RESAMPLE \
	for ( i = frac = 0, j = s_rawend & ( MAX_RAW_SAMPLES - 1 ); \
		  k = frac >> 8, i < outcount; \
		  i++, frac += fracstep, j = ( j + 1 ) & ( MAX_RAW_SAMPLES - 1 ) )

/**
*	@brief	Add raw audio samples to streaming buffer (for music/cinematics playback).
*	@param	samples		Number of input samples.
*	@param	rate		Input sample rate in Hz.
*	@param	width		Sample width in bytes (1=8-bit, 2=16-bit).
*	@param	channels	Number of audio channels (1=mono, 2=stereo).
*	@param	data		Pointer to raw PCM audio data.
*	@param	volume		Playback volume multiplier (0.0 to 1.0).
*	@return	True if samples were successfully queued, false on buffer overflow.
*	@note	Performs sample rate conversion and writes to circular ring buffer.
*	@note	Applies volume scaling during resampling for efficiency.
**/
static bool DMA_RawSamples( int samples, int rate, int width, int channels, const byte *data, float volume ) {
    float stepscale = (float)rate / dma.speed;
    int i, j, k, frac, fracstep = stepscale * 256;
    int outcount = samples / stepscale;
    float vol = snd_vol * volume;

    if (s_rawend < s_paintedtime)
        s_rawend = s_paintedtime;

    if (width == 2) {
        const int16_t *src = (const int16_t *)data;
        if (channels == 2) {
            RESAMPLE {
                s_rawsamples[j].left  = src[k*2+0] * vol;
                s_rawsamples[j].right = src[k*2+1] * vol;
            }
        } else if (channels == 1) {
            RESAMPLE {
                s_rawsamples[j].left  =
                s_rawsamples[j].right = src[k] * vol;
            }
        }
    } else if (width == 1) {
        vol *= 256;
        if (channels == 2) {
            RESAMPLE {
                s_rawsamples[j].left  = (data[k*2+0] - 128) * vol;
                s_rawsamples[j].right = (data[k*2+1] - 128) * vol;
            }
        } else if (channels == 1) {
            RESAMPLE {
                s_rawsamples[j].left  =
                s_rawsamples[j].right = (data[k] - 128) * vol;
            }
	}

	// Update ring buffer write position.
	s_rawend += outcount;
	return true;
}

#undef RESAMPLE

/**
*	@brief	Check if streaming buffer needs more raw samples.
*	@return	True if buffer has room for at least 2048 more samples, false if nearly full.
**/
static bool DMA_NeedRawSamples( void ) {
	return s_rawend - s_paintedtime < MAX_RAW_SAMPLES - 2048;
}

/**
*	@brief	Clear streaming buffer and reset write position.
*	@note	Called when stopping music/cinematic playback.
**/
static void DMA_DropRawSamples( void ) {
	memset( s_rawsamples, 0, sizeof( s_rawsamples ) );
	s_rawend = s_paintedtime;
}

/**
*
*
*
*	Paint Buffer Transfer (Software Mixing Output):
*
*
*
**/

/**
*	@brief	Transfer mixed audio from paint buffer to DMA buffer (optimized 16-bit stereo).
*	@param	samp		Source sample pairs from paint buffer.
*	@param	endtime		Target paint time to write up to.
*	@note	Handles circular DMA buffer wrapping and sample clipping.
*	@note	Optimized fast path for 16-bit stereo output.
**/
static void TransferStereo16( samplepair_t *samp, int endtime ) {
	int ltime = s_paintedtime;
	int size = dma.samples >> 1;

	while ( ltime < endtime ) {
		// Handle recirculating buffer wrapping.
		int lpos = ltime & ( size - 1 );
		int count = min( size - lpos, endtime - ltime );

		// Write a linear blast of samples to DMA buffer.
		int16_t *out = ( int16_t * )dma.buffer + ( lpos << 1 );
		for ( int i = 0; i < count; i++, samp++, out += 2 ) {
			out[0] = clip16( samp->left );
			out[1] = clip16( samp->right );
		}

		ltime += count;
	}
}

/**
*	@brief	Transfer mixed audio from paint buffer to DMA buffer (general case).
*	@param	samp		Source sample pairs from paint buffer.
*	@param	endtime		Target paint time to write up to.
*	@note	Handles arbitrary sample formats (8/16-bit, mono/stereo).
*	@note	Performs sample format conversion and circular buffer wrapping.
**/
static void TransferStereo( samplepair_t *samp, int endtime ) {
	float *p = ( float * )samp;
	int count = ( endtime - s_paintedtime ) * dma.channels;
	int out_mask = dma.samples - 1;
	int out_idx = s_paintedtime * dma.channels & out_mask;
	int step = 3 - dma.channels;
	int val;

	if ( dma.samplebits == 16 ) {
		// 16-bit output format.
		int16_t *out = ( int16_t * )dma.buffer;
		while ( count-- ) {
			val = *p;
			p += step;
			out[out_idx] = clip16( val );
			out_idx = ( out_idx + 1 ) & out_mask;
		}
	} else if ( dma.samplebits == 8 ) {
		// 8-bit output format (convert from signed to unsigned).
		uint8_t *out = ( uint8_t * )dma.buffer;
		while ( count-- ) {
			val = *p;
			p += step;
			out[out_idx] = ( clip16( val ) >> 8 ) + 128;
			out_idx = ( out_idx + 1 ) & out_mask;
		}
	}
}

/**
*	@brief	Transfer paint buffer to DMA hardware buffer with optional test tone and stereo swap.
*	@param	samp		Source sample pairs from paint buffer.
*	@param	endtime		Target paint time to write up to.
*	@note	Applies test tone generation if s_testsound is enabled.
*	@note	Swaps left/right channels if s_swapstereo is enabled.
*	@note	Selects optimized transfer function based on output format.
**/
static void TransferPaintBuffer( samplepair_t *samp, int endtime ) {
	int i;

	// Generate test tone if enabled (sine wave for audio pipeline debugging).
	if ( s_testsound->integer ) {
		for ( i = 0; i < endtime - s_paintedtime; i++ ) {
			samp[i].left = samp[i].right = sin( ( s_paintedtime + i ) * 0.1f ) * 20000;
		}
	}

	// Swap stereo channels if enabled.
	if ( s_swapstereo->integer ) {
		for ( i = 0; i < endtime - s_paintedtime; i++ ) {
			SWAP( float, samp[i].left, samp[i].right );
		}
	}

	// Select optimized or general transfer function based on output format.
	if ( dma.samplebits == 16 && dma.channels == 2 ) {
		// Optimized path for 16-bit stereo.
		TransferStereo16( samp, endtime );
	} else {
		// General case for other formats.
		TransferStereo( samp, endtime );
	}
}

/**
*
*
*
*	Underwater Audio Filter (High Shelf Biquad):
*
*
*
**/

//! History state for biquad filter (one per stereo channel).
typedef struct {
	float z1, z2;	//! Delay line samples for IIR filter.
} hist_t;

//! Filter history for left and right channels.
static hist_t hist[2];

//! Biquad filter coefficients (computed from gain parameter).
static float a1, a2, b0, b1, b2;

/**
*	@brief	Initialize biquad filter coefficients for underwater high-frequency attenuation.
*	@param	hfGain	High-frequency gain (0.0 to 1.0, where 1.0 is no attenuation).
*	@note	Implements "high shelf" biquad filter matching OpenAL Soft AL_FILTER_LOWPASS.
*	@note	Center frequency is 5000 Hz with 1.0 bandwidth (Q = sqrt(2)/2).
**/
static void s_underwater_gain_hf_changed( float hfGain ) {
	float f0norm = 5000.0f / dma.speed;
	
	// Clamp gain to valid range.
	if ( hfGain < 0 ) {
		hfGain = 0;
	} else if ( hfGain > 1 ) {
		hfGain = 1;
	}
	float gain = hfGain;

	// Limit to -60dB to prevent numerical instability.
	gain = max( gain, 0.001f );

	// Calculate biquad coefficients using analog prototype method.
	float w0 = M_PI * 2.0f * f0norm;
	float sin_w0 = sin( w0 );
	float cos_w0 = cos( w0 );
	float alpha = sin_w0 / 2.0f * M_SQRT2;
	float sqrtgain_alpha_2 = 2.0f * sqrtf( gain ) * alpha;
	float a0;

	// Numerator coefficients (zeros).
	b0 = gain * ( ( gain + 1.0f ) + ( gain - 1.0f ) * cos_w0 + sqrtgain_alpha_2 );
	b1 = gain * ( ( gain - 1.0f ) + ( gain + 1.0f ) * cos_w0 ) * -2.0f;
	b2 = gain * ( ( gain + 1.0f ) + ( gain - 1.0f ) * cos_w0 - sqrtgain_alpha_2 );

	// Denominator coefficients (poles).
	a0 = ( gain + 1.0f ) - ( gain - 1.0f ) * cos_w0 + sqrtgain_alpha_2;
	a1 = ( ( gain - 1.0f ) - ( gain + 1.0f ) * cos_w0 ) * 2.0f;
	a2 = ( gain + 1.0f ) - ( gain - 1.0f ) * cos_w0 - sqrtgain_alpha_2;

	// Normalize coefficients by a0.
	a1 /= a0; a2 /= a0; b0 /= a0; b1 /= a0; b2 /= a0;
}

/**
*	@brief	Apply biquad filter to single audio channel using Direct Form I topology.
*	@param	hist	Filter history structure containing delay line samples.
*	@param	samp	Pointer to sample buffer (stride of 2 for stereo interleaving).
*	@param	count	Number of samples to process.
*	@note	Uses transposed Direct Form II structure for numerical stability.
**/
static void filter_ch( hist_t *hist, float *samp, int count ) {
	float z1 = hist->z1;
	float z2 = hist->z2;

	// Apply biquad filter equation: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2].
	for ( int i = 0; i < count; i++, samp += 2 ) {
		float input = *samp;
		float output = input * b0 + z1;
		z1 = input * b1 - output * a1 + z2;
		z2 = input * b2 - output * a2;
		*samp = output;
	}

	// Save delay line state for next call.
	hist->z1 = z1;
	hist->z2 = z2;
}

/**
*	@brief	Apply underwater low-pass filter to both stereo channels.
*	@param	samp	Sample buffer containing stereo pairs.
*	@param	count	Number of sample pairs to filter.
*	@note	Called during mixing when s_underwater cvar indicates player is submerged.
**/
static void underwater_filter( samplepair_t *samp, int count ) {
	// Apply biquad filter to left channel.
	filter_ch( &hist[0], &samp->left, count );
	// Apply biquad filter to right channel.
	filter_ch( &hist[1], &samp->right, count );
}

/**
*
*
*
*	Channel Mixing (Software Audio Rendering):
*
*
*
**/

//! Function pointer type for channel paint functions (mix sound into buffer).
typedef void ( *paintfunc_t )( channel_t *, sfxcache_t *, int, samplepair_t * );

/**
*	Macro to declare paint function with standard signature.
*	ch = channel structure, sc = sound cache, count = samples to mix, samp = destination buffer.
**/
#define PAINTFUNC( name ) \
	static void name( channel_t *ch, sfxcache_t *sc, int count, samplepair_t *samp )

/**
*	@brief	Mix mono 8-bit sound into stereo paint buffer.
*	@param	ch		Channel structure containing volume and playback position.
*	@param	sc		Sound cache structure containing audio data.
*	@param	count	Number of samples to mix.
*	@param	samp	Destination sample pair buffer.
*	@note	Converts unsigned 8-bit to signed, applies volume, and accumulates to buffer.
**/
PAINTFUNC( PaintMono8 ) {
	float leftvol = ch->leftvol * snd_vol * 256;
	float rightvol = ch->rightvol * snd_vol * 256;
	uint8_t *sfx = sc->data + ch->pos;

	// Mix unsigned 8-bit samples (convert to signed, apply volume, accumulate).
	for ( int i = 0; i < count; i++, samp++, sfx++ ) {
		samp->left += ( *sfx - 128 ) * leftvol;
		samp->right += ( *sfx - 128 ) * rightvol;
	}
}

/**
*	@brief	Mix stereo 8-bit sound into stereo paint buffer with downmix.
*	@param	ch		Channel structure containing volume and playback position.
*	@param	sc		Sound cache structure containing audio data.
*	@param	count	Number of samples to mix.
*	@param	samp	Destination sample pair buffer.
*	@note	Sums left and right channels, applies sqrt(0.5) compensation for energy preservation.
**/
PAINTFUNC( PaintStereoDmix8 ) {
	float leftvol = ch->leftvol * snd_vol * ( 256 * M_SQRT1_2 );
	float rightvol = ch->rightvol * snd_vol * ( 256 * M_SQRT1_2 );
	uint8_t *sfx = sc->data + ch->pos * 2;

	// Downmix stereo to mono then spatialize.
	for ( int i = 0; i < count; i++, samp++, sfx += 2 ) {
		int sum = ( sfx[0] - 128 ) + ( sfx[1] - 128 );
		samp->left += sum * leftvol;
		samp->right += sum * rightvol;
	}
}

/**
*	@brief	Mix stereo 8-bit sound into stereo paint buffer (full stereo).
*	@param	ch		Channel structure containing volume and playback position.
*	@param	sc		Sound cache structure containing audio data.
*	@param	count	Number of samples to mix.
*	@param	samp	Destination sample pair buffer.
*	@note	Preserves stereo image, applies volume, accumulates to buffer.
**/
PAINTFUNC( PaintStereoFull8 ) {
	float vol = ch->leftvol * snd_vol * 256;
	uint8_t *sfx = sc->data + ch->pos * 2;

	// Mix stereo samples preserving stereo image.
	for ( int i = 0; i < count; i++, samp++, sfx += 2 ) {
		samp->left += ( sfx[0] - 128 ) * vol;
		samp->right += ( sfx[1] - 128 ) * vol;
	}
}

/**
*	@brief	Mix mono 16-bit sound into stereo paint buffer.
*	@param	ch		Channel structure containing volume and playback position.
*	@param	sc		Sound cache structure containing audio data.
*	@param	count	Number of samples to mix.
*	@param	samp	Destination sample pair buffer.
*	@note	Applies volume and accumulates signed 16-bit samples to buffer.
**/
PAINTFUNC( PaintMono16 ) {
	float leftvol = ch->leftvol * snd_vol;
	float rightvol = ch->rightvol * snd_vol;
	int16_t *sfx = ( int16_t * )sc->data + ch->pos;

	// Mix signed 16-bit mono samples.
	for ( int i = 0; i < count; i++, samp++, sfx++ ) {
		samp->left += *sfx * leftvol;
		samp->right += *sfx * rightvol;
	}
}

/**
*	@brief	Mix stereo 16-bit sound into stereo paint buffer with downmix.
*	@param	ch		Channel structure containing volume and playback position.
*	@param	sc		Sound cache structure containing audio data.
*	@param	count	Number of samples to mix.
*	@param	samp	Destination sample pair buffer.
*	@note	Sums left and right channels, applies sqrt(0.5) compensation.
**/
PAINTFUNC( PaintStereoDmix16 ) {
	float leftvol = ch->leftvol * snd_vol * M_SQRT1_2;
	float rightvol = ch->rightvol * snd_vol * M_SQRT1_2;
	int16_t *sfx = ( int16_t * )sc->data + ch->pos * 2;

	// Downmix stereo to mono then spatialize.
	for ( int i = 0; i < count; i++, samp++, sfx += 2 ) {
		int sum = sfx[0] + sfx[1];
		samp->left += sum * leftvol;
		samp->right += sum * rightvol;
	}
}

/**
*	@brief	Mix stereo 16-bit sound into stereo paint buffer (full stereo).
*	@param	ch		Channel structure containing volume and playback position.
*	@param	sc		Sound cache structure containing audio data.
*	@param	count	Number of samples to mix.
*	@param	samp	Destination sample pair buffer.
*	@note	Preserves stereo image, applies volume, accumulates to buffer.
**/
PAINTFUNC( PaintStereoFull16 ) {
	float vol = ch->leftvol * snd_vol;
	int16_t *sfx = ( int16_t * )sc->data + ch->pos * 2;

	// Mix stereo samples preserving stereo image.
	for ( int i = 0; i < count; i++, samp++, sfx += 2 ) {
		samp->left += sfx[0] * vol;
		samp->right += sfx[1] * vol;
	}
}

//! Lookup table for paint functions indexed by [width-1][channels-1].
static const paintfunc_t paintfuncs[] = {
	PaintMono8,
	PaintStereoDmix8,
	PaintStereoFull8,
	PaintMono16,
	PaintStereoDmix16,
	PaintStereoFull16,
};

/**
*	@brief	Mix all active sound channels into paint buffer and transfer to DMA.
*	@param	endtime	Target paint time to render up to.
*	@note	Processes sound channels in blocks of PAINTBUFFER_SIZE samples.
*	@note	Applies underwater filter if player is submerged.
*	@note	Mixes raw streaming samples (music/cinematics) into output.
**/
static void PaintChannels( int endtime ) {
	samplepair_t paintbuffer[PAINTBUFFER_SIZE];
	channel_t *ch;
	int i;
	bool underwater = S_IsUnderWater();

	while ( s_paintedtime < endtime ) {
		// Limit paint block size to buffer capacity.
		int64_t end = min( endtime, s_paintedtime + PAINTBUFFER_SIZE );

		// Issue playsounds that are due to start in this block.
		while ( 1 ) {
			playsound_t *ps = PS_FIRST( &s_pendingplays );
			if ( PS_TERM( ps, &s_pendingplays ) )
				break;    // No more pending sounds.
			if ( ps->begin > s_paintedtime ) {
				end = min( end, ps->begin );  // Stop before next playsound.
				break;
			}
			S_IssuePlaysound( ps );
		}

		// Clear the paint buffer for this block.
		memset( paintbuffer, 0, ( end - s_paintedtime ) * sizeof( samplepair_t ) );

		// Copy from the raw streaming sound source (music/cinematics).
		int stop = min( end, s_rawend );
		for ( i = s_paintedtime; i < stop; i++ )
			paintbuffer[i - s_paintedtime] = s_rawsamples[i & ( MAX_RAW_SAMPLES - 1 )];

		// Paint (mix) all active sound channels into buffer.
		for ( i = 0, ch = s_channels; i < s_numchannels; i++, ch++ ) {
			int64_t ltime = s_paintedtime;

			while ( ltime < end ) {
				// Skip silent or inactive channels.
				if ( !ch->sfx || ( !ch->leftvol && !ch->rightvol ) )
					break;

				// Load sound effect cache.
				sfxcache_t *sc = S_LoadSound( ch->sfx );
				if ( !sc )
					break;

				Q_assert( sc->width == 1 || sc->width == 2 );
				Q_assert( sc->channels == 1 || sc->channels == 2 );

				// Calculate samples to paint (limited by channel end and block end).
				int64_t count = min( end, ch->end ) - ltime;

				if ( count > 0 ) {
					// Select appropriate paint function based on format and spatialization.
					int func = ( sc->width - 1 ) * 3 + ( sc->channels - 1 ) * ( S_IsFullVolume( ch ) + 1 );
					paintfuncs[func]( ch, sc, count, &paintbuffer[ltime - s_paintedtime] );
					ch->pos += count;
					ltime += count;
				}

				// Handle channel looping or termination.
				if ( ltime >= ch->end ) {
					if ( ch->autosound ) {
						// Autolooping sounds always restart from beginning.
						ch->pos = 0;
						ch->end = ltime + sc->length;
					} else if ( sc->loopstart >= 0 ) {
						// Loop sound from specified loop point.
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					} else {
						// Channel finished, stop playback.
						ch->sfx = nullptr;
					}
				}
			}
		}

		// Apply underwater filter if player is submerged.
		if ( s_rawend >= s_paintedtime ) {
			int stop = min( end, s_rawend );

			if ( underwater )
				underwater_filter( paintbuffer, stop - s_paintedtime );

			// Mix raw streaming samples into paint buffer.
			for ( int i = s_paintedtime; i < stop; i++ ) {
				int s = i & ( MAX_RAW_SAMPLES - 1 );
				paintbuffer[i - s_paintedtime].left += s_rawsamples[s].left;
				paintbuffer[i - s_paintedtime].right += s_rawsamples[s].right;
			}
		}

		// Transfer paint buffer to DMA hardware buffer.
		TransferPaintBuffer( paintbuffer, end );
		s_paintedtime = end;
	}
}

/**
*	@brief	Update cached volume multiplier when s_volume cvar changes.
*	@param	self	Pointer to s_volume cvar.
**/
static void s_volume_changed( cvar_t *self ) {
	snd_vol = S_GetLinearVolume( Cvar_ClampValue( self, 0, 1 ) );
}

/**
*
/**
*
*
*
*	Initialization and Shutdown:
*
*
*
**/

// Platform-specific DMA driver declarations.
#ifdef _WIN32
extern const snddma_driver_t snddma_wave;
#endif

#if USE_SDL
extern const snddma_driver_t snddma_sdl;
#endif

//! Array of available DMA drivers (platform-specific).
static const snddma_driver_t *const s_drivers[] = {
#ifdef _WIN32
	&snddma_wave,
#endif
#if USE_SDL
	&snddma_sdl,
#endif
	nullptr
};

//! Currently active DMA driver instance.
static snddma_driver_t snddma;

/**
*	@brief	Display DMA buffer configuration and status information.
**/
static void DMA_SoundInfo( void ) {
	Com_Printf( "%5d channels\n", dma.channels );
	Com_Printf( "%5d samples\n", dma.samples );
	Com_Printf( "%5d samplepos\n", dma.samplepos );
	Com_Printf( "%5d samplebits\n", dma.samplebits );
	Com_Printf( "%5d submission_chunk\n", dma.submission_chunk );
	Com_Printf( "%5d speed\n", dma.speed );
	Com_Printf( "%p dma buffer\n", dma.buffer );
}

/**
*	@brief	Initialize DMA sound backend and select appropriate driver.
*	@return	True if initialization succeeded, false on error.
*	@note	Attempts to initialize user-specified driver first, then falls back to others.
*	@note	Initializes cvars, underwater filter, and volume settings.
**/
static bool DMA_Init( void ) {
	sndinitstat_t ret = SIS_FAILURE;
	int i;

	// Register console variables.
	s_khz = Cvar_Get( "s_khz", "44", CVAR_ARCHIVE | CVAR_SOUND );
	s_mixahead = Cvar_Get( "s_mixahead", "0.1", CVAR_ARCHIVE );
	s_testsound = Cvar_Get( "s_testsound", "0", 0 );
	s_swapstereo = Cvar_Get( "s_swapstereo", "0", 0 );
	cvar_t *s_driver = Cvar_Get( "s_driver", "", CVAR_SOUND );

	// Try user-specified driver first.
	for ( i = 0; s_drivers[i]; i++ ) {
		if ( !strcmp( s_drivers[i]->name, s_driver->string ) ) {
			snddma = *s_drivers[i];
			ret = snddma.init();
			break;
		}
	}

	// Fall back to trying all available drivers.
	if ( ret != SIS_SUCCESS ) {
		int tried = i;
		for ( i = 0; s_drivers[i]; i++ ) {
			if ( i == tried )
				continue;
			snddma = *s_drivers[i];
			if ( ( ret = snddma.init() ) == SIS_SUCCESS )
				break;
		}
		Cvar_Reset( s_driver );
	}

	if ( ret != SIS_SUCCESS )
		return false;

	// Initialize underwater filter coefficients.
	s_underwater_gain_hf_changed( 0.25f );

	// Set up volume change callback.
	s_volume->changed = s_volume_changed;
	s_volume_changed( s_volume );

	// Allocate maximum channels for software mixing.
	s_numchannels = MAX_CHANNELS;

	Com_Printf( "sound sampling rate: %i\n", dma.speed );

	return true;
}

/**
*	@brief	Shutdown DMA sound backend and release resources.
**/
static void DMA_Shutdown( void ) {
	snddma.shutdown();
	s_numchannels = 0;

	s_volume->changed = nullptr;
}

/**
*	@brief	Activate DMA backend (called when window gains focus).
**/
static void DMA_Activate( void ) {
	if ( snddma.activate ) {
		S_StopAllSounds();
		snddma.activate( s_active );
	}
}

/**
*
*
*
*	Frame Updates and Audio Mixing:
*
*
*
**/

/**
*	@brief	Calculate begin offset for sound playback with drift compensation.
*	@param	timeofs	Time offset in seconds from current server time.
*	@return	Sample index when sound should begin playing.
*	@note	Implements drift correction to keep client audio synchronized with server.
*	@note	Prevents excessive drift (limited to ±0.3 seconds).
**/
static int DMA_DriftBeginofs( float timeofs ) {
	static int s_beginofs;
	int start;

	// Calculate ideal start time with accumulated drift offset.
	start = cl.servertime * 0.001f * dma.speed + s_beginofs;

	// If lagging behind, reset to current paint time.
	if ( start < s_paintedtime ) {
		start = s_paintedtime;
		s_beginofs = start - ( cl.servertime * 0.001f * dma.speed );
	// If too far ahead, clamp drift.
	} else if ( start > s_paintedtime + 0.3f * dma.speed ) {
		start = s_paintedtime + 0.1f * dma.speed;
		s_beginofs = start - ( cl.servertime * 0.001f * dma.speed );
	// Otherwise, apply gradual drift correction.
	} else {
		s_beginofs -= 10;
	}

	return timeofs ? start + timeofs * dma.speed : s_paintedtime;
}

/**
*	@brief	Clear DMA hardware buffer (silence all audio).
**/
static void DMA_ClearBuffer( void ) {
	snddma.begin_painting();
	if ( dma.buffer )
		memset( dma.buffer, dma.samplebits == 8 ? 0x80 : 0, dma.samples * dma.samplebits / 8 );
	snddma.submit();
}

/**
*	@brief	Calculate stereo panning and distance attenuation for 3D sound source.
*	@param	origin		3D position of sound source in world coordinates.
*	@param	master_vol	Master volume multiplier (0.0 to 1.0).
*	@param	dist_mult	Distance attenuation multiplier.
*	@param	left_vol	Output left channel volume.
*	@param	right_vol	Output right channel volume.
*	@note	Uses dot product with listener's right vector for stereo panning.
*	@note	Applies linear distance attenuation starting at SOUND_FULLVOLUME distance.
**/
static void SpatializeOrigin( const vec3_t origin, float master_vol, float dist_mult, float *left_vol, float *right_vol ) {
	vec_t dot;
	vec_t dist;
	vec_t lscale, rscale, scale;
	vec3_t source_vec;

	// Calculate stereo separation and distance attenuation.
	VectorSubtract( origin, cl.listener_spatialize.origin, source_vec );

	dist = VectorNormalize( source_vec );
	dist -= SOUND_FULLVOLUME;
	if ( dist < 0 )
		dist = 0;           // Close enough to be at full volume.
	dist *= dist_mult;      // Apply attenuation multiplier.

	// Calculate stereo panning (mono output has no panning).
	if ( dma.channels == 1 ) {
		rscale = 1.0f;
		lscale = 1.0f;
	} else {
		// Use dot product with listener's right vector for panning.
		dot = DotProduct( cl.listener_spatialize.v_right, source_vec );
		rscale = 0.5f * ( 1.0f + dot );
		lscale = 0.5f * ( 1.0f - dot );
	}

	// Combine distance attenuation with stereo panning.
	scale = ( 1.0f - dist ) * rscale;
	*right_vol = master_vol * scale;
	if ( *right_vol < 0 )
		*right_vol = 0;

	scale = ( 1.0f - dist ) * lscale;
	*left_vol = master_vol * scale;
	if ( *left_vol < 0 )
		*left_vol = 0;
}

/**
*	@brief	Apply 3D spatialization to sound channel.
*	@param	ch	Channel structure containing source position and volume parameters.
*	@note	View entity sounds are always full volume (no attenuation or panning).
*	@note	Updates ch->leftvol and ch->rightvol based on spatial calculation.
**/
static void DMA_Spatialize( channel_t *ch ) {
	vec3_t origin;

	// View entity sounds are always full volume (no attenuation/spatialization).
	if ( S_IsFullVolume( ch ) ) {
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	// Determine sound source position.
	if ( ch->fixed_origin ) {
		VectorCopy( ch->origin, origin );
	} else {
		CL_GetEntitySoundOrigin( ch->entnum, origin );
	}

	// Calculate stereo panning and distance attenuation.
	SpatializeOrigin( origin, ch->master_vol, ch->dist_mult, &ch->leftvol, &ch->rightvol );
}

/**
*	@brief	Update and add looping ambient sounds for visible entities.
*	@note	Entities with ->sound field generate looped sounds.
*	@note	Automatically started, stopped, and merged as entities are sent to client.
*	@note	DMA backend merges identical looping sounds into single channel.
**/
static void AddLoopSounds( void ) {
    int         i, j;
    int         sounds[MAX_EDICTS];
    float       left, right, left_total, right_total;
    channel_t   *ch;
    sfx_t       *sfx;
    sfxcache_t  *sc;
    int         num;
    entity_state_t  *ent;
    vec3_t      origin;

    if (cls.state != ca_active || !s_active || sv_paused->integer || !s_ambient->integer)
        return;

    S_BuildSoundList(sounds);

    for (i = 0; i < cl.frame.numEntities; i++) {
        if (!sounds[i])
            continue;

        sfx = S_SfxForHandle(cl.sound_precache[sounds[i]]);
        if (!sfx)
            continue;       // bad sound effect
        sc = sfx->cache;
        if (!sc)
            continue;

        num = (cl.frame.firstEntity + i) & PARSE_ENTITIES_MASK;
        ent = &cl.entityStates[num];

        // find the total contribution of all sounds of this type
        CL_GetEntitySoundOrigin(ent->number, origin);
        SpatializeOrigin(origin, 1.0f, SOUND_LOOPATTENUATE,
                         &left_total, &right_total);
        for (j = i + 1; j < cl.frame.numEntities; j++) {
            if (sounds[j] != sounds[i])
                continue;
            sounds[j] = 0;  // don't check this again later

            num = (cl.frame.firstEntity + j) & PARSE_ENTITIES_MASK;
            ent = &cl.entityStates[num];

            CL_GetEntitySoundOrigin(ent->number, origin);
            SpatializeOrigin(origin, 1.0f, SOUND_LOOPATTENUATE,
                             &left, &right);
            left_total += left;
            right_total += right;
        }

        if (left_total == 0 && right_total == 0)
            continue;       // not audible

        // allocate a channel
        ch = S_PickChannel(0, 0);
        if (!ch)
            return;

        ch->leftvol = min(left_total, 1.0f);
        ch->rightvol = min(right_total, 1.0f);
        ch->master_vol = 1.0f;
        ch->dist_mult = SOUND_LOOPATTENUATE;    // for S_IsFullVolume()
        ch->autosound = true;   // remove next frame
        ch->sfx = sfx;
        ch->pos = s_paintedtime % sc->length;
        ch->end = s_paintedtime + sc->length - ch->pos;
    }
}

static int DMA_GetTime(void)
{
    static int      buffers;
    static int      oldsamplepos;
    int fullsamples = dma.samples >> (dma.channels - 1);

// it is possible to miscount buffers if it has wrapped twice between
// calls to S_Update.  Oh well.
    if (dma.samplepos < oldsamplepos) {
        buffers++;      // buffer wrapped
        if (s_paintedtime > 0x40000000) {
            // time to chop things off to avoid 32 bit limits
            buffers = 0;
            s_paintedtime = fullsamples;
            S_StopAllSounds();
        }
    }
    oldsamplepos = dma.samplepos;

    return buffers * fullsamples + (dma.samplepos >> (dma.channels - 1));
}

static void DMA_Update(void)
{
    int         i;
    channel_t   *ch;
    int         samples, soundtime, endtime;

    // update spatialization for dynamic sounds
    for (i = 0, ch = s_channels; i < s_numchannels; i++, ch++) {
        if (!ch->sfx)
            continue;

        if (ch->autosound) {
            // autosounds are regenerated fresh each frame
            memset(ch, 0, sizeof(*ch));
            continue;
        }

        DMA_Spatialize(ch);     // respatialize channel
        if (!ch->leftvol && !ch->rightvol) {
            memset(ch, 0, sizeof(*ch));
            continue;
        }
    }

    // add loopsounds
    AddLoopSounds();

#if USE_DEBUG
    if (s_show->integer) {
        int total = 0;
        for (i = 0, ch = s_channels; i < s_numchannels; i++, ch++) {
            if (ch->sfx && (ch->leftvol || ch->rightvol)) {
                Com_Printf("%.3f %.3f %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
                total++;
            }
        }
        if (s_show->integer > 1 || total) {
            Com_Printf("----(%i)---- painted: %i\n", total, s_paintedtime);
        }
    }
#endif

    snddma.begin_painting();

    if (!dma.buffer)
        return;

    // update DMA time
    soundtime = DMA_GetTime();

    // check to make sure that we haven't overshot
    if (s_paintedtime < soundtime) {
        Com_DPrintf("%s: overflow\n", __func__);
        s_paintedtime = soundtime;
    }

    // mix ahead of current position
    endtime = soundtime + Cvar_ClampValue(s_mixahead, 0, 1) * dma.speed;

    // mix to an even submission block size
    endtime = ALIGN(endtime, dma.submission_chunk);
    samples = dma.samples >> (dma.channels - 1);
    endtime = min(endtime, soundtime + samples);

    PaintChannels(endtime);

    snddma.submit();
}

const sndapi_t snd_dma = {
    .init = DMA_Init,
    .shutdown = DMA_Shutdown,
    .update = DMA_Update,
    .activate = DMA_Activate,
    .sound_info = DMA_SoundInfo,
    .upload_sfx = DMA_UploadSfx,
    .set_eax_effect_properties = DMA_SetEAXEFfectProperties,
    .page_in_sfx = DMA_PageInSfx,
    .raw_samples = DMA_RawSamples,
    .need_raw_samples = DMA_NeedRawSamples,
    .drop_raw_samples = DMA_DropRawSamples,
    .get_begin_ofs = DMA_DriftBeginofs,
    .play_channel = DMA_Spatialize,
    .stop_all_sounds = DMA_ClearBuffer,
};
