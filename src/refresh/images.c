/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2003-2008 Andrey Nazarov
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

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

//
// images.c -- image reading and writing functions
//

#include "shared/shared.h"
#include "common/async.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#include "../client/cl_client.h"
#include "refresh/images.h"
#include "system/system.h"
#include "format/pcx.h"
#include "format/wal.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "vkpt/dds.h"

#include <assert.h>

#define R_COLORMAP_PCX    "pics/colormap.pcx"

#define IMG_LOAD(x) \
    static int IMG_Load##x(byte *rawdata, size_t rawlen, \
        image_t *image, byte **pic)

static int IMG_LoadDDS(byte *rawdata, size_t rawlen, image_t *image, byte **pic);

void stbi_write(void *context, void *data, int size)
{
	fwrite(data, size, 1, ((screenshot_t *) context)->fp);
}

extern cvar_t* vid_rtx;
extern cvar_t* gl_use_hd_assets;

/*
====================================================================

IMAGE FLOOD FILLING

====================================================================
*/

typedef struct {
    short       x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP(off, dx, dy) \
    do { \
        if (pos[off] == fillcolor) { \
            pos[off] = 255; \
            fifo[inpt].x = x + (dx); \
            fifo[inpt].y = y + (dy); \
            inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
        } else if (pos[off] != 255) { \
            fdc = pos[off]; \
        } \
    } while(0)

/*
=================
IMG_FloodFill

Fill background pixels so mipmapping doesn't have haloes
=================
*/
static q_noinline void IMG_FloodFill(byte *skin, int skinwidth, int skinheight)
{
    byte                fillcolor = *skin; // assume this is the pixel to fill
    floodfill_t         fifo[FLOODFILL_FIFO_SIZE];
    int                 inpt = 0, outpt = 0;
    int                 filledcolor = 0; // FIXME: fixed black

    // can't fill to filled color or to transparent color
    // (used as visited marker)
    if (fillcolor == filledcolor || fillcolor == 255) {
        return;
    }

    fifo[inpt].x = 0, fifo[inpt].y = 0;
    inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

    while (outpt != inpt) {
        int         x = fifo[outpt].x, y = fifo[outpt].y;
        int         fdc = filledcolor;
        byte        *pos = &skin[x + skinwidth * y];

        outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

        if (x > 0) FLOODFILL_STEP(-1, -1, 0);
        if (x < skinwidth - 1) FLOODFILL_STEP(1, 1, 0);
        if (y > 0) FLOODFILL_STEP(-skinwidth, 0, -1);
        if (y < skinheight - 1) FLOODFILL_STEP(skinwidth, 0, 1);

        skin[x + skinwidth * y] = fdc;
    }
}

/*
=================================================================

PCX LOADING

=================================================================
*/

static int IMG_DecodePCX(byte *rawdata, size_t rawlen, byte *pixels,
                         byte *palette, int *width, int *height)
{
    byte    *raw, *end;
    dpcx_t  *pcx;
    int     x, y, w, h, scan;
    int     dataByte, runLength;

    //
    // parse the PCX file
    //
    if (rawlen < sizeof(dpcx_t)) {
        return Q_ERR_FILE_TOO_SMALL;
    }

    pcx = (dpcx_t *)rawdata;

    if (pcx->manufacturer != 10 || pcx->version != 5) {
        return Q_ERR_UNKNOWN_FORMAT;
    }

    if (pcx->encoding != 1 || pcx->bits_per_pixel != 8) {
        Com_SetLastError("invalid encoding or bits per pixel");
        return Q_ERR_INVALID_FORMAT;
    }

    w = (LittleShort(pcx->xmax) - LittleShort(pcx->xmin)) + 1;
    h = (LittleShort(pcx->ymax) - LittleShort(pcx->ymin)) + 1;
    if (w < 1 || h < 1 || w > 640 || h > 480) {
        Com_SetLastError("invalid image dimensions");
        return Q_ERR_INVALID_FORMAT;
    }

    if (pcx->color_planes != 1) {
        Com_SetLastError("invalid number of color planes");
        return Q_ERR_INVALID_FORMAT;
    }

    scan = LittleShort(pcx->bytes_per_line);
    if (scan < w) {
        Com_SetLastError("invalid number of bytes per line");
        return Q_ERR_INVALID_FORMAT;
    }

    //
    // get palette
    //
    if (palette) {
        if (rawlen < 768) {
            return Q_ERR_FILE_TOO_SMALL;
        }
        memcpy(palette, (byte *)pcx + rawlen - 768, 768);
    }

    //
    // get pixels
    //
    if (pixels) {
        raw = pcx->data;
        end = (byte *)pcx + rawlen;
        for (y = 0; y < h; y++, pixels += w) {
            for (x = 0; x < scan;) {
                if (raw >= end)
                    return Q_ERR_BAD_RLE_PACKET;
                dataByte = *raw++;

                if ((dataByte & 0xC0) == 0xC0) {
                    runLength = dataByte & 0x3F;
                    if (x + runLength > scan)
                        return Q_ERR_BAD_RLE_PACKET;
                    if (raw >= end)
                        return Q_ERR_BAD_RLE_PACKET;
                    dataByte = *raw++;
                } else {
                    runLength = 1;
                }

                while (runLength--) {
                    if (x < w)
                        pixels[x] = dataByte;
                    x++;
                }
            }
        }
    }

    if (width)
        *width = w;
    if (height)
        *height = h;

    return Q_ERR_SUCCESS;
}

/*
===============
IMG_Unpack8
===============
*/
static int IMG_Unpack8(uint32_t *out, const uint8_t *in, int width, int height)
{
    int         x, y, p;
    bool        has_alpha = false;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            p = *in;
            if (p == 255) {
                has_alpha = true;
                // transparent, so scan around for another color
                // to avoid alpha fringes
                if (y > 0 && *(in - width) != 255)
                    p = *(in - width);
                else if (y < height - 1 && *(in + width) != 255)
                    p = *(in + width);
                else if (x > 0 && *(in - 1) != 255)
                    p = *(in - 1);
                else if (x < width - 1 && *(in + 1) != 255)
                    p = *(in + 1);
                else if (y > 0 && x > 0 && *(in - width - 1) != 255)
                    p = *(in - width - 1);
                else if (y > 0 && x < width - 1 && *(in - width + 1) != 255)
                    p = *(in - width + 1);
                else if (y < height - 1 && x > 0 && *(in + width - 1) != 255)
                    p = *(in + width - 1);
                else if (y < height - 1 && x < width - 1 && *(in + width + 1) != 255)
                    p = *(in + width + 1);
                else
                    p = 0;
                // copy rgb components
                *out = d_8to24table[p] & U32_RGB;
            } else {
                *out = d_8to24table[p];
            }
            in++;
            out++;
        }
    }

    if (has_alpha)
        return IF_PALETTED | IF_TRANSPARENT;

    return IF_PALETTED | IF_OPAQUE;
}

IMG_LOAD(PCX)
{
    byte        buffer[640 * 480];
    int         w, h;
    int         ret;

    ret = IMG_DecodePCX(rawdata, rawlen, buffer, NULL, &w, &h);
    if (ret < 0)
        return ret;

    if (image->type == IT_SKIN)
        IMG_FloodFill(buffer, w, h);

    *pic = IMG_AllocPixels(w * h * 4);

    image->upload_width = image->width = w;
    image->upload_height = image->height = h;
    image->pixel_format = PF_R8G8B8A8_UNORM;
    image->pix_data_size = (size_t)w * (size_t)h * 4u;
    image->mip_levels = 0;
    image->flags |= IMG_Unpack8((uint32_t *)*pic, buffer, w, h);

    return Q_ERR_SUCCESS;
}


/*
=================================================================

WAL LOADING

=================================================================
*/

IMG_LOAD(WAL)
{
    miptex_t    *mt;
    unsigned    w, h, offset, size, endpos;

    if (rawlen < sizeof(miptex_t)) {
        return Q_ERR_FILE_TOO_SMALL;
    }

    mt = (miptex_t *)rawdata;

    w = LittleLong(mt->width);
    h = LittleLong(mt->height);
    if (w < 1 || h < 1 || w > MAX_TEXTURE_SIZE || h > MAX_TEXTURE_SIZE) {
        Com_SetLastError("invalid image dimensions");
        return Q_ERR_INVALID_FORMAT;
    }

    size = w * h;

    offset = LittleLong(mt->offsets[0]);
    endpos = offset + size;
    if (endpos < offset || endpos > rawlen) {
        return Q_ERR_BAD_EXTENT;
    }

    *pic = IMG_AllocPixels(size * 4);

    image->upload_width = image->width = w;
    image->upload_height = image->height = h;
    image->pixel_format = PF_R8G8B8A8_UNORM;
    image->pix_data_size = (size_t)size * 4u;
    image->mip_levels = 0;
    image->flags |= IMG_Unpack8((uint32_t *)*pic, (uint8_t *)mt + offset, w, h);

    return Q_ERR_SUCCESS;
}

/*
====================================================================

DDS LOADING

====================================================================
*/

typedef struct {
    uint32_t    mask;
    int         shift;
    int         bits;
} dds_channel_t;

typedef enum {
    DDS_LOAD_RGBA8,
    DDS_LOAD_GRAY16,
    DDS_LOAD_BC1,
    DDS_LOAD_BC2,
    DDS_LOAD_BC3,
    DDS_LOAD_BC4,
    DDS_LOAD_BC5
} dds_load_mode_t;

static inline uint16_t DDS_ReadLE16(const byte *src)
{
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static inline uint32_t DDS_ReadLE32(const byte *src)
{
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static inline uint64_t DDS_ReadLE48(const byte *src)
{
    return (uint64_t)src[0] |
           ((uint64_t)src[1] << 8) |
           ((uint64_t)src[2] << 16) |
           ((uint64_t)src[3] << 24) |
           ((uint64_t)src[4] << 32) |
           ((uint64_t)src[5] << 40);
}

static inline uint64_t DDS_ReadLE64(const byte *src)
{
    return (uint64_t)src[0] |
           ((uint64_t)src[1] << 8) |
           ((uint64_t)src[2] << 16) |
           ((uint64_t)src[3] << 24) |
           ((uint64_t)src[4] << 32) |
           ((uint64_t)src[5] << 40) |
           ((uint64_t)src[6] << 48) |
           ((uint64_t)src[7] << 56);
}

static inline byte DDS_Expand5(uint32_t value)
{
    return (byte)((value << 3) | (value >> 2));
}

static inline byte DDS_Expand6(uint32_t value)
{
    return (byte)((value << 2) | (value >> 4));
}

static inline byte DDS_ScaleToByte(uint32_t value, int bits)
{
    if (bits <= 0) {
        return 0;
    }

    if (bits >= 8) {
        return (byte)(value >> (bits - 8));
    }

    const uint32_t max_value = (1u << bits) - 1u;
    return (byte)((value * 255u + (max_value / 2u)) / max_value);
}

static inline uint16_t DDS_ScaleToU16(uint32_t value, int bits)
{
    if (bits <= 0) {
        return 0;
    }

    if (bits >= 16) {
        return (uint16_t)value;
    }

    const uint32_t max_value = (1u << bits) - 1u;
    return (uint16_t)((value * 65535u + (max_value / 2u)) / max_value);
}

static dds_channel_t DDS_MakeChannel(uint32_t mask)
{
    dds_channel_t channel = { mask, 0, 0 };

    if (!mask) {
        return channel;
    }

    while ((mask & 1u) == 0u) {
        mask >>= 1;
        channel.shift++;
    }

    while ((mask & 1u) != 0u) {
        mask >>= 1;
        channel.bits++;
    }

    return channel;
}

static inline byte DDS_ExtractChannelByte(uint32_t raw, const dds_channel_t *channel)
{
    if (!channel->bits) {
        return 0;
    }

    return DDS_ScaleToByte((raw & channel->mask) >> channel->shift, channel->bits);
}

static inline uint16_t DDS_ExtractChannelU16(uint32_t raw, const dds_channel_t *channel)
{
    if (!channel->bits) {
        return 0;
    }

    return DDS_ScaleToU16((raw & channel->mask) >> channel->shift, channel->bits);
}

static inline uint32_t DDS_ReadPixel(const byte *src, int bytes_per_pixel)
{
    uint32_t raw = 0;

    for (int i = 0; i < bytes_per_pixel; i++) {
        raw |= (uint32_t)src[i] << (8 * i);
    }

    return raw;
}

static void DDS_DecodeColorPalette(byte palette[4][4], uint16_t color0, uint16_t color1, bool allow_transparent)
{
    palette[0][0] = DDS_Expand5((color0 >> 11) & 0x1f);
    palette[0][1] = DDS_Expand6((color0 >> 5) & 0x3f);
    palette[0][2] = DDS_Expand5(color0 & 0x1f);
    palette[0][3] = 255;

    palette[1][0] = DDS_Expand5((color1 >> 11) & 0x1f);
    palette[1][1] = DDS_Expand6((color1 >> 5) & 0x3f);
    palette[1][2] = DDS_Expand5(color1 & 0x1f);
    palette[1][3] = 255;

    if (allow_transparent && color0 <= color1) {
        palette[2][0] = (byte)((palette[0][0] + palette[1][0]) / 2);
        palette[2][1] = (byte)((palette[0][1] + palette[1][1]) / 2);
        palette[2][2] = (byte)((palette[0][2] + palette[1][2]) / 2);
        palette[2][3] = 255;

        palette[3][0] = 0;
        palette[3][1] = 0;
        palette[3][2] = 0;
        palette[3][3] = 0;
        return;
    }

    palette[2][0] = (byte)((2 * palette[0][0] + palette[1][0]) / 3);
    palette[2][1] = (byte)((2 * palette[0][1] + palette[1][1]) / 3);
    palette[2][2] = (byte)((2 * palette[0][2] + palette[1][2]) / 3);
    palette[2][3] = 255;

    palette[3][0] = (byte)((palette[0][0] + 2 * palette[1][0]) / 3);
    palette[3][1] = (byte)((palette[0][1] + 2 * palette[1][1]) / 3);
    palette[3][2] = (byte)((palette[0][2] + 2 * palette[1][2]) / 3);
    palette[3][3] = 255;
}

static void DDS_DecodeBC1Block(byte *dst, int width, int height, int base_x, int base_y, const byte *block, bool *has_alpha)
{
    byte palette[4][4];
    uint16_t color0 = DDS_ReadLE16(block);
    uint16_t color1 = DDS_ReadLE16(block + 2);
    uint32_t indices = DDS_ReadLE32(block + 4);

    DDS_DecodeColorPalette(palette, color0, color1, true);

    for (int y = 0; y < 4; y++) {
        int dst_y = base_y + y;
        if (dst_y >= height) {
            break;
        }

        for (int x = 0; x < 4; x++) {
            int dst_x = base_x + x;
            if (dst_x >= width) {
                break;
            }

            unsigned index = (indices >> (2 * (4 * y + x))) & 3u;
            if (palette[index][3] != 255) {
                *has_alpha = true;
            }
            memcpy(dst + (dst_y * width + dst_x) * 4, palette[index], 4);
        }
    }
}

static void DDS_DecodeBC2Block(byte *dst, int width, int height, int base_x, int base_y, const byte *block, bool *has_alpha)
{
    byte palette[4][4];
    uint64_t alpha_bits = DDS_ReadLE64(block);
    uint16_t color0 = DDS_ReadLE16(block + 8);
    uint16_t color1 = DDS_ReadLE16(block + 10);
    uint32_t indices = DDS_ReadLE32(block + 12);

    DDS_DecodeColorPalette(palette, color0, color1, false);

    for (int y = 0; y < 4; y++) {
        int dst_y = base_y + y;
        if (dst_y >= height) {
            break;
        }

        for (int x = 0; x < 4; x++) {
            int dst_x = base_x + x;
            if (dst_x >= width) {
                break;
            }

            int dst_pixel = (dst_y * width + dst_x) * 4;
            int alpha_index = 4 * (4 * y + x);
            byte alpha = (byte)(((alpha_bits >> alpha_index) & 0xFu) * 17u);
            unsigned index = (indices >> (2 * (4 * y + x))) & 3u;

            if (alpha != 255) {
                *has_alpha = true;
            }
            memcpy(dst + dst_pixel, palette[index], 4);
            dst[dst_pixel + 3] = alpha;
        }
    }
}

static void DDS_DecodeAlphaValues(int alpha[8], const byte *block, bool signed_mode)
{
    int alpha0 = signed_mode ? (int)(int8_t)block[0] : block[0];
    int alpha1 = signed_mode ? (int)(int8_t)block[1] : block[1];

    alpha[0] = alpha0;
    alpha[1] = alpha1;

    if (alpha0 > alpha1) {
        for (int i = 1; i < 7; i++) {
            alpha[i + 1] = ((7 - i) * alpha0 + i * alpha1) / 7;
        }
        return;
    }

    for (int i = 1; i < 5; i++) {
        alpha[i + 1] = ((5 - i) * alpha0 + i * alpha1) / 5;
    }

    alpha[6] = signed_mode ? -128 : 0;
    alpha[7] = signed_mode ? 127 : 255;
}

static void DDS_DecodeAlphaPixels(byte alpha[16], const byte *block, bool signed_mode)
{
    int values[8];
    DDS_DecodeAlphaValues(values, block, signed_mode);
    uint64_t indices = DDS_ReadLE48(block + 2);

    for (int i = 0; i < 16; i++) {
        int value = values[(indices >> (3 * i)) & 7u];
        if (signed_mode) {
            value += 128;
        }
        if (value < 0) {
            value = 0;
        } else if (value > 255) {
            value = 255;
        }
        alpha[i] = (byte)value;
    }
}

static void DDS_DecodeBC3Block(byte *dst, int width, int height, int base_x, int base_y, const byte *block, bool *has_alpha)
{
    byte palette[4][4];
    byte alpha[16];
    uint16_t color0 = DDS_ReadLE16(block + 8);
    uint16_t color1 = DDS_ReadLE16(block + 10);
    uint32_t indices = DDS_ReadLE32(block + 12);

    DDS_DecodeAlphaPixels(alpha, block, false);
    DDS_DecodeColorPalette(palette, color0, color1, false);

    for (int y = 0; y < 4; y++) {
        int dst_y = base_y + y;
        if (dst_y >= height) {
            break;
        }

        for (int x = 0; x < 4; x++) {
            int dst_x = base_x + x;
            if (dst_x >= width) {
                break;
            }

            int dst_pixel = (dst_y * width + dst_x) * 4;
            unsigned index = (indices >> (2 * (4 * y + x))) & 3u;

            if (alpha[4 * y + x] != 255) {
                *has_alpha = true;
            }
            memcpy(dst + dst_pixel, palette[index], 4);
            dst[dst_pixel + 3] = alpha[4 * y + x];
        }
    }
}

static void DDS_DecodeBC4Block(uint16_t *dst, int width, int height, int base_x, int base_y, const byte *block, bool signed_mode)
{
    byte alpha[16];

    DDS_DecodeAlphaPixels(alpha, block, signed_mode);

    for (int y = 0; y < 4; y++) {
        int dst_y = base_y + y;
        if (dst_y >= height) {
            break;
        }

        for (int x = 0; x < 4; x++) {
            int dst_x = base_x + x;
            if (dst_x >= width) {
                break;
            }

            dst[dst_y * width + dst_x] = (uint16_t)alpha[4 * y + x] * 257u;
        }
    }
}

static void DDS_DecodeBC5Block(byte *dst, int width, int height, int base_x, int base_y, const byte *block, bool signed_mode)
{
    byte red[16];
    byte green[16];

    DDS_DecodeAlphaPixels(red, block, signed_mode);
    DDS_DecodeAlphaPixels(green, block + 8, signed_mode);

    for (int y = 0; y < 4; y++) {
        int dst_y = base_y + y;
        if (dst_y >= height) {
            break;
        }

        for (int x = 0; x < 4; x++) {
            int dst_x = base_x + x;
            if (dst_x >= width) {
                break;
            }

            int pixel = (dst_y * width + dst_x) * 4;
            dst[pixel + 0] = red[4 * y + x];
            dst[pixel + 1] = green[4 * y + x];
            dst[pixel + 2] = 255;
            dst[pixel + 3] = 255;
        }
    }
}

static size_t DDS_CompressedBlockBytes(pixelformat_t format)
{
    switch (format) {
    case PF_BC1:
    case PF_BC4_UNORM:
    case PF_BC4_SNORM:
        return 8;
    case PF_BC2:
    case PF_BC3:
    case PF_BC5_UNORM:
    case PF_BC5_SNORM:
    case PF_BC6H_UFLOAT:
    case PF_BC6H_SFLOAT:
    case PF_BC7:
        return 16;
    default:
        return 0;
    }
}

static size_t DDS_CompressedPayloadSize(int width, int height, uint32_t mip_levels, pixelformat_t format)
{
    size_t block_bytes = DDS_CompressedBlockBytes(format);
    if (!block_bytes || width < 1 || height < 1 || mip_levels < 1) {
        return 0;
    }

    size_t payload_size = 0;
    int level_width = width;
    int level_height = height;

    for (uint32_t mip = 0; mip < mip_levels; mip++) {
        size_t blocks_w = (size_t)(level_width + 3) / 4;
        size_t blocks_h = (size_t)(level_height + 3) / 4;
        payload_size += blocks_w * blocks_h * block_bytes;

        if (level_width > 1) {
            level_width >>= 1;
        }
        if (level_height > 1) {
            level_height >>= 1;
        }
    }

    return payload_size;
}

static int IMG_LoadDDS(byte *rawdata, size_t rawlen, image_t *image, byte **pic)
{
    const DDS_HEADER *dds;
    const DDS_HEADER_DXT10 *dxt10 = NULL;
    size_t header_size = sizeof(DDS_HEADER);
    int width;
    int height;
    pixelformat_t pixel_format = PF_R8G8B8A8_UNORM;
    uint32_t mip_levels = 1;
    dds_load_mode_t mode;
    bool signed_mode = false;
    dds_channel_t r = { 0 }, g = { 0 }, b = { 0 }, a = { 0 };
    bool use_luminance = false;
    bool alpha_only = false;
    bool has_alpha = false;
    bool compressed = false;
    byte *pixels;

    if (rawlen < sizeof(DDS_HEADER)) {
        return Q_ERR_FILE_TOO_SMALL;
    }

    dds = (DDS_HEADER *)rawdata;
    width = LittleLong(dds->width);
    height = LittleLong(dds->height);

    if (dds->magic != DDS_MAGIC || dds->size != sizeof(DDS_HEADER) - 4) {
        return Q_ERR_UNKNOWN_FORMAT;
    }

    if (width < 1 || height < 1 || width > MAX_TEXTURE_SIZE || height > MAX_TEXTURE_SIZE) {
        Com_SetLastError("invalid image dimensions");
        return Q_ERR_INVALID_FORMAT;
    }

    if ((dds->caps2 & DDS_CUBEMAP) != 0 || (dds->flags & DDS_FLAGS_VOLUME) != 0 || dds->depth > 1) {
        Com_SetLastError("DDS cubemaps, arrays, and volume textures are not supported here");
        return Q_ERR_INVALID_FORMAT;
    }

    mip_levels = LittleLong(dds->mipMapCount);
    if (mip_levels == 0) {
        mip_levels = 1;
    }

    if (dds->ddspf.flags & DDS_FOURCC) {
        switch (dds->ddspf.fourCC) {
        case MAKEFOURCC('D', 'X', '1', '0'):
            if (rawlen < sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10)) {
                return Q_ERR_FILE_TOO_SMALL;
            }

            dxt10 = (DDS_HEADER_DXT10 *)(rawdata + sizeof(DDS_HEADER));
            header_size += sizeof(DDS_HEADER_DXT10);

            if (dxt10->resourceDimension != DDS_DIMENSION_TEXTURE2D || dxt10->arraySize != 1 || (dxt10->miscFlag & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0) {
                Com_SetLastError("DDS DX10 arrays, cubes, and non-2D textures are not supported here");
                return Q_ERR_INVALID_FORMAT;
            }

            switch (dxt10->dxgiFormat) {
            case DXGI_FORMAT_BC1_TYPELESS:
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC1_UNORM_SRGB:
                pixel_format = PF_BC1;
                compressed = true;
                break;
            case DXGI_FORMAT_BC2_TYPELESS:
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC2_UNORM_SRGB:
                pixel_format = PF_BC2;
                compressed = true;
                break;
            case DXGI_FORMAT_BC3_TYPELESS:
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC3_UNORM_SRGB:
                pixel_format = PF_BC3;
                compressed = true;
                break;
            case DXGI_FORMAT_BC4_TYPELESS:
            case DXGI_FORMAT_BC4_UNORM:
                pixel_format = PF_BC4_UNORM;
                compressed = true;
                break;
            case DXGI_FORMAT_BC4_SNORM:
                pixel_format = PF_BC4_SNORM;
                compressed = true;
                break;
            case DXGI_FORMAT_BC5_TYPELESS:
            case DXGI_FORMAT_BC5_UNORM:
                pixel_format = PF_BC5_UNORM;
                compressed = true;
                break;
            case DXGI_FORMAT_BC5_SNORM:
                pixel_format = PF_BC5_SNORM;
                compressed = true;
                break;
            case DXGI_FORMAT_BC6H_TYPELESS:
            case DXGI_FORMAT_BC6H_UF16:
                pixel_format = PF_BC6H_UFLOAT;
                compressed = true;
                break;
            case DXGI_FORMAT_BC6H_SF16:
                pixel_format = PF_BC6H_SFLOAT;
                compressed = true;
                break;
            case DXGI_FORMAT_BC7_TYPELESS:
            case DXGI_FORMAT_BC7_UNORM:
            case DXGI_FORMAT_BC7_UNORM_SRGB:
                pixel_format = PF_BC7;
                compressed = true;
                break;
            default:
                break;
            }
            break;
        case MAKEFOURCC('D', 'X', 'T', '1'):
            pixel_format = PF_BC1;
            compressed = true;
            break;
        case MAKEFOURCC('D', 'X', 'T', '2'):
        case MAKEFOURCC('D', 'X', 'T', '3'):
            pixel_format = PF_BC2;
            compressed = true;
            break;
        case MAKEFOURCC('D', 'X', 'T', '4'):
        case MAKEFOURCC('D', 'X', 'T', '5'):
            pixel_format = PF_BC3;
            compressed = true;
            break;
        case MAKEFOURCC('A', 'T', 'I', '1'):
        case MAKEFOURCC('B', 'C', '4', 'U'):
            pixel_format = PF_BC4_UNORM;
            compressed = true;
            break;
        case MAKEFOURCC('B', 'C', '4', 'S'):
            pixel_format = PF_BC4_SNORM;
            compressed = true;
            break;
        case MAKEFOURCC('A', 'T', 'I', '2'):
        case MAKEFOURCC('B', 'C', '5', 'U'):
            pixel_format = PF_BC5_UNORM;
            compressed = true;
            break;
        case MAKEFOURCC('B', 'C', '5', 'S'):
            pixel_format = PF_BC5_SNORM;
            compressed = true;
            break;
        default:
            break;
        }
    }

    if (compressed) {
        size_t payload_size = DDS_CompressedPayloadSize(width, height, mip_levels, pixel_format);
        if (!payload_size || rawlen < header_size + payload_size) {
            return Q_ERR_BAD_EXTENT;
        }

        *pic = IMG_AllocPixels(payload_size);
        memcpy(*pic, rawdata + header_size, payload_size);

        image->upload_width = image->width = width;
        image->upload_height = image->height = height;
        image->pixel_format = pixel_format;
        image->pix_data_size = payload_size;
        image->mip_levels = mip_levels;
#if USE_REF == REF_VKPT
        image->skip_runtime_normalization = true;
#endif

        if (pixel_format == PF_BC4_UNORM || pixel_format == PF_BC4_SNORM ||
            pixel_format == PF_BC5_UNORM || pixel_format == PF_BC5_SNORM ||
            pixel_format == PF_BC6H_UFLOAT || pixel_format == PF_BC6H_SFLOAT) {
            image->flags |= IF_OPAQUE;
        }

        return Q_ERR_SUCCESS;
    }

    if (dds->ddspf.flags & DDS_FOURCC) {
        switch (dds->ddspf.fourCC) {
        case MAKEFOURCC('D', 'X', '1', '0'):
            if (rawlen < sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10)) {
                return Q_ERR_FILE_TOO_SMALL;
            }

            dxt10 = (DDS_HEADER_DXT10 *)(rawdata + sizeof(DDS_HEADER));
            header_size += sizeof(DDS_HEADER_DXT10);

            if (dxt10->resourceDimension != DDS_DIMENSION_TEXTURE2D || dxt10->arraySize != 1 || (dxt10->miscFlag & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0) {
                Com_SetLastError("DDS DX10 arrays, cubes, and non-2D textures are not supported here");
                return Q_ERR_INVALID_FORMAT;
            }

            switch (dxt10->dxgiFormat) {
            case DXGI_FORMAT_BC1_TYPELESS:
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC1_UNORM_SRGB:
                mode = DDS_LOAD_BC1;
                break;
            case DXGI_FORMAT_BC2_TYPELESS:
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC2_UNORM_SRGB:
                mode = DDS_LOAD_BC2;
                break;
            case DXGI_FORMAT_BC3_TYPELESS:
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC3_UNORM_SRGB:
                mode = DDS_LOAD_BC3;
                break;
            case DXGI_FORMAT_BC4_TYPELESS:
            case DXGI_FORMAT_BC4_UNORM:
                mode = DDS_LOAD_BC4;
                break;
            case DXGI_FORMAT_BC4_SNORM:
                mode = DDS_LOAD_BC4;
                signed_mode = true;
                break;
            case DXGI_FORMAT_BC5_TYPELESS:
            case DXGI_FORMAT_BC5_UNORM:
                mode = DDS_LOAD_BC5;
                break;
            case DXGI_FORMAT_BC5_SNORM:
                mode = DDS_LOAD_BC5;
                signed_mode = true;
                break;
            case DXGI_FORMAT_R8_UNORM:
                mode = DDS_LOAD_GRAY16;
                r = DDS_MakeChannel(0xffu);
                use_luminance = true;
                break;
            case DXGI_FORMAT_R16_UNORM:
                mode = DDS_LOAD_GRAY16;
                r = DDS_MakeChannel(0xffffu);
                use_luminance = true;
                break;
            case DXGI_FORMAT_R8G8_UNORM:
                mode = DDS_LOAD_RGBA8;
                r = DDS_MakeChannel(0x00ffu);
                g = DDS_MakeChannel(0xff00u);
                break;
            case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                mode = DDS_LOAD_RGBA8;
                r = DDS_MakeChannel(0x000000ffu);
                g = DDS_MakeChannel(0x0000ff00u);
                b = DDS_MakeChannel(0x00ff0000u);
                a = DDS_MakeChannel(0xff000000u);
                break;
            case DXGI_FORMAT_B8G8R8A8_TYPELESS:
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
                mode = DDS_LOAD_RGBA8;
                r = DDS_MakeChannel(0x00ff0000u);
                g = DDS_MakeChannel(0x0000ff00u);
                b = DDS_MakeChannel(0x000000ffu);
                a = DDS_MakeChannel(0xff000000u);
                break;
            case DXGI_FORMAT_A8_UNORM:
                mode = DDS_LOAD_RGBA8;
                a = DDS_MakeChannel(0xffu);
                alpha_only = true;
                break;
            default:
                Com_SetLastError("unsupported DDS DX10 pixel format");
                return Q_ERR_INVALID_FORMAT;
            }
            break;
        case MAKEFOURCC('D', 'X', 'T', '1'):
            mode = DDS_LOAD_BC1;
            break;
        case MAKEFOURCC('D', 'X', 'T', '2'):
        case MAKEFOURCC('D', 'X', 'T', '3'):
            mode = DDS_LOAD_BC2;
            break;
        case MAKEFOURCC('D', 'X', 'T', '4'):
        case MAKEFOURCC('D', 'X', 'T', '5'):
            mode = DDS_LOAD_BC3;
            break;
        case MAKEFOURCC('A', 'T', 'I', '1'):
        case MAKEFOURCC('B', 'C', '4', 'U'):
            mode = DDS_LOAD_BC4;
            break;
        case MAKEFOURCC('B', 'C', '4', 'S'):
            mode = DDS_LOAD_BC4;
            signed_mode = true;
            break;
        case MAKEFOURCC('A', 'T', 'I', '2'):
        case MAKEFOURCC('B', 'C', '5', 'U'):
            mode = DDS_LOAD_BC5;
            break;
        case MAKEFOURCC('B', 'C', '5', 'S'):
            mode = DDS_LOAD_BC5;
            signed_mode = true;
            break;
        default:
            Com_SetLastError("unsupported DDS fourCC format");
            return Q_ERR_INVALID_FORMAT;
        }
    } else {
        int channel_count;
        int bytes_per_pixel;
        size_t pitch;
        size_t required;
        bool is_luminance = (dds->ddspf.flags & DDS_LUMINANCE) != 0;

        r = DDS_MakeChannel(dds->ddspf.RBitMask);
        g = DDS_MakeChannel(dds->ddspf.GBitMask);
        b = DDS_MakeChannel(dds->ddspf.BBitMask);
        a = DDS_MakeChannel(dds->ddspf.ABitMask);

        channel_count = !!r.bits + !!g.bits + !!b.bits;
        bytes_per_pixel = dds->ddspf.RGBBitCount / 8;

        if (dds->ddspf.flags & DDS_ALPHA) {
            mode = DDS_LOAD_RGBA8;
            alpha_only = true;
            if (!a.bits && bytes_per_pixel == 1) {
                a = DDS_MakeChannel(0xffu);
            }
        } else if ((is_luminance && !a.bits) || (channel_count == 1 && !a.bits)) {
            mode = DDS_LOAD_GRAY16;
            use_luminance = true;
            if (!r.bits && !g.bits && !b.bits && bytes_per_pixel == 1) {
                r = DDS_MakeChannel(0xffu);
            }
        } else {
            mode = DDS_LOAD_RGBA8;
            use_luminance = is_luminance;
        }

        if (bytes_per_pixel < 1 || bytes_per_pixel > 4 || (dds->ddspf.RGBBitCount % 8) != 0) {
            Com_SetLastError("unsupported DDS bit depth");
            return Q_ERR_INVALID_FORMAT;
        }

        pitch = (((size_t)width * dds->ddspf.RGBBitCount) + 31u) / 32u * 4u;
        required = pitch * (size_t)height;
        if (rawlen < header_size || rawlen - header_size < required) {
            return Q_ERR_BAD_EXTENT;
        }

        if (mode == DDS_LOAD_GRAY16) {
            pixels = IMG_AllocPixels((size_t)width * (size_t)height * sizeof(uint16_t));
            *pic = pixels;

            for (int y = 0; y < height; y++) {
                const byte *src_row = rawdata + header_size + pitch * (size_t)y;
                uint16_t *dst_row = ((uint16_t *)pixels) + (size_t)y * width;

                for (int x = 0; x < width; x++) {
                    uint32_t raw = DDS_ReadPixel(src_row + x * bytes_per_pixel, bytes_per_pixel);
                    const dds_channel_t *gray_channel = r.bits ? &r : (g.bits ? &g : &b);
                    uint16_t value = DDS_ExtractChannelU16(raw, gray_channel);
                    dst_row[x] = value;
                }
            }

            image->pixel_format = PF_R16_UNORM;
            image->upload_width = image->width = width;
            image->upload_height = image->height = height;
            image->pix_data_size = (size_t)width * (size_t)height * sizeof(uint16_t);
            image->mip_levels = 0;
#if USE_REF == REF_VKPT
            image->skip_runtime_normalization = true;
#endif
            image->flags |= IF_OPAQUE;
            return Q_ERR_SUCCESS;
        }

        if (mode == DDS_LOAD_RGBA8) {
            pixels = IMG_AllocPixels((size_t)width * (size_t)height * 4u);
            *pic = pixels;

            for (int y = 0; y < height; y++) {
                const byte *src_row = rawdata + header_size + pitch * (size_t)y;
                byte *dst_row = pixels + (size_t)y * width * 4u;

                for (int x = 0; x < width; x++) {
                    uint32_t raw = DDS_ReadPixel(src_row + x * bytes_per_pixel, bytes_per_pixel);
                    byte rgba[4];

                    if (alpha_only) {
                        rgba[0] = 255;
                        rgba[1] = 255;
                        rgba[2] = 255;
                        rgba[3] = a.bits ? DDS_ExtractChannelByte(raw, &a) : (byte)(raw & 0xffu);
                    } else if (use_luminance) {
                        const dds_channel_t *gray_channel = r.bits ? &r : (g.bits ? &g : &b);
                        byte lum = DDS_ExtractChannelByte(raw, gray_channel);
                        rgba[0] = lum;
                        rgba[1] = lum;
                        rgba[2] = lum;
                        rgba[3] = a.bits ? DDS_ExtractChannelByte(raw, &a) : 255;
                    } else {
                        rgba[0] = DDS_ExtractChannelByte(raw, &r);
                        rgba[1] = DDS_ExtractChannelByte(raw, &g);
                        rgba[2] = DDS_ExtractChannelByte(raw, &b);
                        rgba[3] = a.bits ? DDS_ExtractChannelByte(raw, &a) : 255;
                    }

                    if (rgba[3] != 255) {
                        has_alpha = true;
                    }

                    memcpy(dst_row + x * 4u, rgba, 4);
                }
            }

            image->pixel_format = PF_R8G8B8A8_UNORM;
            image->upload_width = image->width = width;
            image->upload_height = image->height = height;
            image->pix_data_size = (size_t)width * (size_t)height * 4u;
            image->mip_levels = 0;
#if USE_REF == REF_VKPT
            image->skip_runtime_normalization = true;
#endif
            if (!has_alpha) {
                image->flags |= IF_OPAQUE;
            }
            return Q_ERR_SUCCESS;
        }

        Com_SetLastError("unsupported DDS uncompressed layout");
        return Q_ERR_INVALID_FORMAT;
    }

    /*
    *   Compressed DDS data uses 4x4 blocks, so validate the block count before decoding.
    */
    size_t blocks_w = ((size_t)width + 3u) / 4u;
    size_t blocks_h = ((size_t)height + 3u) / 4u;
    size_t block_size = 0;
    size_t required = 0;

    switch (mode) {
    case DDS_LOAD_BC1:
    case DDS_LOAD_BC4:
        block_size = 8u;
        break;
    case DDS_LOAD_BC2:
    case DDS_LOAD_BC3:
    case DDS_LOAD_BC5:
        block_size = 16u;
        break;
    default:
        return Q_ERR_INVALID_FORMAT;
    }

    required = blocks_w * blocks_h * block_size;
    if (rawlen < header_size || rawlen - header_size < required) {
        return Q_ERR_BAD_EXTENT;
    }

    if (mode == DDS_LOAD_BC4) {
        pixels = IMG_AllocPixels((size_t)width * (size_t)height * sizeof(uint16_t));
        *pic = pixels;
    } else {
        pixels = IMG_AllocPixels((size_t)width * (size_t)height * 4u);
        *pic = pixels;
    }

    for (size_t by = 0; by < blocks_h; by++) {
        for (size_t bx = 0; bx < blocks_w; bx++) {
            const byte *block = rawdata + header_size + (by * blocks_w + bx) * block_size;
            int block_x = (int)(bx * 4u);
            int block_y = (int)(by * 4u);

            if (mode == DDS_LOAD_BC1) {
                DDS_DecodeBC1Block(pixels, width, height, block_x, block_y, block, &has_alpha);
            } else if (mode == DDS_LOAD_BC2) {
                DDS_DecodeBC2Block(pixels, width, height, block_x, block_y, block, &has_alpha);
            } else if (mode == DDS_LOAD_BC3) {
                DDS_DecodeBC3Block(pixels, width, height, block_x, block_y, block, &has_alpha);
            } else if (mode == DDS_LOAD_BC4) {
                DDS_DecodeBC4Block((uint16_t *)pixels, width, height, block_x, block_y, block, signed_mode);
            } else if (mode == DDS_LOAD_BC5) {
                DDS_DecodeBC5Block(pixels, width, height, block_x, block_y, block, signed_mode);
            }
        }
    }

    image->upload_width = image->width = width;
    image->upload_height = image->height = height;
    image->pixel_format = (mode == DDS_LOAD_BC4) ? PF_R16_UNORM : PF_R8G8B8A8_UNORM;
    image->pix_data_size = (mode == DDS_LOAD_BC4)
        ? (size_t)width * (size_t)height * sizeof(uint16_t)
        : (size_t)width * (size_t)height * 4u;
    image->mip_levels = 0;
    if (!has_alpha) {
        image->flags |= IF_OPAQUE;
    }

    return Q_ERR_SUCCESS;
}

/*
=================================================================

STB_IMAGE LOADING

=================================================================
*/

static bool supports_extended_pixel_format(void)
{
	return cls.ref_type == REF_TYPE_VKPT;
}

IMG_LOAD(STB)
{
	int w, h, channels;
	byte* data = NULL;
	if(supports_extended_pixel_format())
	{
		int img_comp;
		stbi_info_from_memory(rawdata, rawlen, NULL, NULL, &img_comp);
		bool img_is_16 = stbi_is_16_bit_from_memory(rawdata, rawlen);

		if(img_comp == 1 && img_is_16)
		{
			// Special: 16bpc grayscale
			data = (byte*)stbi_load_16_from_memory(rawdata, rawlen, &w, &h, &channels, 1);
			image->pixel_format = PF_R16_UNORM;
		}
		// else: handle default case (8bpc RGBA) below
	}
	if(!data)
	{
		data = stbi_load_from_memory(rawdata, rawlen, &w, &h, &channels, 4);
		image->pixel_format = PF_R8G8B8A8_UNORM;
	}

	if (!data)
	{
		Com_SetLastError(stbi_failure_reason());
		return Q_ERR_LIBRARY_ERROR;
	}

	*pic = data;

	image->upload_width = image->width = w;
	image->upload_height = image->height = h;
    image->pix_data_size = (size_t)w * (size_t)h * (image->pixel_format == PF_R16_UNORM ? 2u : 4u);
    image->mip_levels = 0;
#if USE_REF == REF_VKPT
    image->skip_runtime_normalization = false;
#endif

	if (channels == 3)
		image->flags |= IF_OPAQUE;

    return Q_ERR_SUCCESS;
}


/*
=================================================================

STB_IMAGE SAVING

=================================================================
*/

static int IMG_SaveTGA(screenshot_t *s)
{
	stbi_flip_vertically_on_write(1);
	int ret = stbi_write_tga_to_func(stbi_write, s, s->width, s->height, 3, s->pixels);

	if (ret) 
		return Q_ERR_SUCCESS;

	Com_SetLastError(stbi_failure_reason());
	return Q_ERR_LIBRARY_ERROR;
}

static int IMG_SaveJPG(screenshot_t *s)
{
	stbi_flip_vertically_on_write(1);
	int ret = stbi_write_jpg_to_func(stbi_write, s, s->width, s->height, 3, s->pixels, s->param);

	if (ret)
		return Q_ERR_SUCCESS;

	Com_SetLastError(stbi_failure_reason());
	return Q_ERR_LIBRARY_ERROR;
}


static int IMG_SavePNG(screenshot_t *s)
{
	stbi_flip_vertically_on_write(1);
	int ret = stbi_write_png_to_func(stbi_write, s, s->width, s->height, 3, s->pixels, s->rowbytes);

	if (ret)
		return Q_ERR_SUCCESS;

	Com_SetLastError(stbi_failure_reason());
	return Q_ERR_LIBRARY_ERROR;
}

static int IMG_SaveHDR(screenshot_t *s)
{
	stbi_flip_vertically_on_write(1);
	// NOTE: The 'pixels' point is byte*, but HDR writing needs float*!
	int ret = stbi_write_hdr_to_func(stbi_write, s, s->width, s->height, 3, (float*)s->pixels);

	if (ret)
		return Q_ERR_SUCCESS;

	Com_SetLastError(stbi_failure_reason());
	return Q_ERR_LIBRARY_ERROR;
}

/*
=========================================================

SCREEN SHOTS

=========================================================
*/

static cvar_t *r_screenshot_format;
static cvar_t *r_screenshot_quality;
static cvar_t *r_screenshot_async;
static cvar_t* r_screenshot_compression;
static cvar_t* r_screenshot_message;
static cvar_t *r_screenshot_template;

static int suffix_pos(const char *s, int ch)
{
    int pos = strlen(s);
    while (pos > 0 && s[pos - 1] == ch)
        pos--;
    return pos;
}

static int parse_template(cvar_t *var, char *buffer, size_t size)
{
    if (FS_NormalizePathBuffer(buffer, var->string, size) < size) {
        FS_CleanupPath(buffer);
        int start = suffix_pos(buffer, 'X');
        int width = strlen(buffer) - start;
        buffer[start] = 0;
        if (width >= 3 && width <= 9)
            return width;
    }

    Com_WPrintf("Bad value '%s' for '%s'. Falling back to '%s'.\n",
                var->string, var->name, var->default_string);
    Cvar_Reset(var);
    Q_strlcpy(buffer, "quake", size);
    return 3;
}

static int create_screenshot(char *buffer, size_t size, FILE **f,
                             const char *name, const char *ext)
{
    char temp[MAX_OSPATH];
    int i, ret, width, count;

    if (name && *name) {
        // save to user supplied name
        if (FS_NormalizePathBuffer(temp, name, sizeof(temp)) >= sizeof(temp)) {
            return Q_ERR(ENAMETOOLONG);
        }
        FS_CleanupPath(temp);
        if (Q_snprintf(buffer, size, "%s/screenshots/%s%s", fs_gamedir, temp, ext) >= size) {
            return Q_ERR(ENAMETOOLONG);
        }
        if ((ret = FS_CreatePath(buffer)) < 0) {
            return ret;
        }
        if (!(*f = fopen(buffer, "wb"))) {
            return Q_ERRNO;
        }
        return 0;
    }

    width = parse_template(r_screenshot_template, temp, sizeof(temp));

    // create the directory
    if (Q_snprintf(buffer, size, "%s/screenshots/%s", fs_gamedir, temp) >= size) {
        return Q_ERR(ENAMETOOLONG);
    }
    if ((ret = FS_CreatePath(buffer)) < 0) {
        return ret;
    }

    count = 1;
    for (i = 0; i < width; i++)
        count *= 10;

    // find a file name to save it to
    for (i = 0; i < count; i++) {
        if (Q_snprintf(buffer, size, "%s/screenshots/%s%0*d%s", fs_gamedir, temp, width, i, ext) >= size) {
            return Q_ERR(ENAMETOOLONG);
        }
        if ((*f = Q_fopen(buffer, "wxb"))) {
            return 0;
        }
        ret = Q_ERRNO;
        if (ret != Q_ERR(EEXIST)) {
            return ret;
        }
    }
    
    return Q_ERR_OUT_OF_SLOTS;
}

static bool is_render_hdr(void)
{
    return R_IsHDR && R_IsHDR();
}

static void screenshot_work_cb(void *arg)
{
    screenshot_t *s = arg;
    s->status = s->save_cb(s);
}

static void screenshot_done_cb(void *arg)
{
    screenshot_t *s = arg;

    if (fclose(s->fp) && !s->status)
        s->status = Q_ERRNO;
    Z_Free(s->pixels);

    if (s->status < 0) {
        const char *msg;

        if (s->status == Q_ERR_LIBRARY_ERROR && !s->async)
            msg = Com_GetLastError();
        else
            msg = Q_ErrorString(s->status);

        Com_EPrintf("Couldn't write %s: %s\n", s->filename, msg);
        remove(s->filename);
    } else if (r_screenshot_message->integer) {
        Com_Printf("Wrote %s\n", s->filename);
    }

    if (s->async) {
        Z_Free(s->filename);
        Z_Free(s);
    }
}

static void make_screenshot(const char *name, const char *ext,
                            save_cb_t save_cb, bool async, int param)
{
    char        buffer[MAX_OSPATH];
    FILE        *fp;
    int         ret;

    if(is_render_hdr()) {
        Com_WPrintf("Screenshot format not supported in HDR mode\n");
        return;
    }
    ret = create_screenshot(buffer, sizeof(buffer), &fp, name, ext);
    if (ret < 0) {
        Com_EPrintf("Couldn't create screenshot: %s\n", Q_ErrorString(ret));
        return;
    }

    screenshot_t s = {
        .save_cb = save_cb,
        .fp = fp,
        .filename = async ? Z_CopyString(buffer) : buffer,
        .status = -1,
        .param = param,
        .async = async,
    };

    IMG_ReadPixels(&s);

    if (async) {
        asyncwork_t work = {
            .work_cb = screenshot_work_cb,
            .done_cb = screenshot_done_cb,
            .cb_arg = Z_CopyStruct(&s),
        };
        Com_QueueAsyncWork(&work);
    } else {
        screenshot_work_cb(&s);
        screenshot_done_cb(&s);
    }
}

static void make_screenshot_hdr(const char *name, bool async)
{
    char        buffer[MAX_OSPATH];
    int         ret;
    FILE        *fp;

    if(!is_render_hdr()) {
        Com_WPrintf("Screenshot format supported in HDR mode only\n");
        return;
    }

    ret = create_screenshot(buffer, sizeof(buffer), &fp, name, ".hdr");
    if (ret < 0) {
        Com_EPrintf("Couldn't create HDR screenshot: %s\n", Q_ErrorString(ret));
        return;
    }

    screenshot_t s = {
        .save_cb = IMG_SaveHDR,
        .fp = fp,
        .filename = async ? Z_CopyString(buffer) : buffer,
        .status = -1,
        .param = 0,
        .async = async,
    };

    IMG_ReadPixelsHDR(&s);

    if (async) {
        asyncwork_t work = {
            .work_cb = screenshot_work_cb,
            .done_cb = screenshot_done_cb,
            .cb_arg = Z_CopyStruct(&s),
        };
        Com_QueueAsyncWork(&work);
    } else {
        screenshot_work_cb(&s);
        screenshot_done_cb(&s);
    }
}

/*
==================
IMG_ScreenShot_f

Standard function to take a screenshot. Saves in default format unless user
overrides format with a second argument. Screenshot name can't be
specified. This function is always compiled in to give a meaningful warning
if no formats are available.
==================
*/
static void IMG_ScreenShot_f(void)
{
    const char *s;

    if (Cmd_Argc() > 2) {
        Com_Printf("Usage: %s [format]\n", Cmd_Argv(0));
        return;
    }

    if (Cmd_Argc() > 1) {
        s = Cmd_Argv(1);
    } else {
        if(is_render_hdr())
            s = "hdr";
        else
        s = r_screenshot_format->string;
    }

    if (*s == 'h') {
        make_screenshot_hdr(NULL, r_screenshot_async->integer > 0);
        return;
    }

    if (*s == 'j') {
        make_screenshot(NULL, ".jpg", IMG_SaveJPG,
                        r_screenshot_async->integer > 0,
                        r_screenshot_quality->integer);
        return;
    }

    if (*s == 'p') {
        make_screenshot(NULL, ".png", IMG_SavePNG,
                        r_screenshot_async->integer > 0,
                        r_screenshot_compression->integer);
        return;
    }

    make_screenshot(NULL, ".tga", IMG_SaveTGA, r_screenshot_async->integer > 0, 0);
}

/*
==================
IMG_ScreenShotXXX_f

Specialized function to take a screenshot in specified format. Screenshot name
can be also specified, as well as quality and compression options.
==================
*/

static void IMG_ScreenShotTGA_f(void)
{
    if (Cmd_Argc() > 2) {
        Com_Printf("Usage: %s [name]\n", Cmd_Argv(0));
        return;
    }

    make_screenshot(Cmd_Argv(1), ".tga", IMG_SaveTGA, r_screenshot_async->integer > 0, 0);
}

static void IMG_ScreenShotJPG_f(void)
{
    int quality;

    if (Cmd_Argc() > 3) {
        Com_Printf("Usage: %s [name] [quality]\n", Cmd_Argv(0));
        return;
    }

    if (Cmd_Argc() > 2) {
        quality = atoi(Cmd_Argv(2));
    } else {
        quality = r_screenshot_quality->integer;
    }

    make_screenshot(Cmd_Argv(1), ".jpg", IMG_SaveJPG,
                    r_screenshot_async->integer > 0, quality);
}

static void IMG_ScreenShotPNG_f(void)
{
    int compression;

    if (Cmd_Argc() > 3) {
        Com_Printf("Usage: %s [name] [compression]\n", Cmd_Argv(0));
        return;
    }

    if (Cmd_Argc() > 2) {
        compression = atoi(Cmd_Argv(2));
    } else {
        compression = r_screenshot_compression->integer;
    }

    make_screenshot(Cmd_Argv(1), ".png", IMG_SavePNG,
                    r_screenshot_async->integer > 0, compression);
}

static void IMG_ScreenShotHDR_f(void)
{
    if (Cmd_Argc() > 2) {
        Com_Printf("Usage: %s [name]\n", Cmd_Argv(0));
        return;
    }

    make_screenshot_hdr(Cmd_Argv(1), r_screenshot_async->integer > 0);
}

/*
=========================================================

IMAGE PROCESSING

=========================================================
*/

void IMG_ResampleTexture(const byte *in, int inwidth, int inheight,
                         byte *out, int outwidth, int outheight)
{
    int i, j;
    const byte  *inrow1, *inrow2;
    unsigned    frac, fracstep;
    unsigned    p1[MAX_TEXTURE_SIZE], p2[MAX_TEXTURE_SIZE];
    const byte  *pix1, *pix2, *pix3, *pix4;
    float       heightScale;

    if (outwidth > MAX_TEXTURE_SIZE) {
        Com_Error(ERR_FATAL, "%s: outwidth > %d", __func__, MAX_TEXTURE_SIZE);
    }

    fracstep = inwidth * 0x10000 / outwidth;

    frac = fracstep >> 2;
    for (i = 0; i < outwidth; i++) {
        p1[i] = 4 * (frac >> 16);
        frac += fracstep;
    }
    frac = 3 * (fracstep >> 2);
    for (i = 0; i < outwidth; i++) {
        p2[i] = 4 * (frac >> 16);
        frac += fracstep;
    }

    heightScale = (float)inheight / outheight;
    inwidth <<= 2;
    for (i = 0; i < outheight; i++) {
        inrow1 = in + inwidth * (int)((i + 0.25f) * heightScale);
        inrow2 = in + inwidth * (int)((i + 0.75f) * heightScale);
        for (j = 0; j < outwidth; j++) {
            pix1 = inrow1 + p1[j];
            pix2 = inrow1 + p2[j];
            pix3 = inrow2 + p1[j];
            pix4 = inrow2 + p2[j];
            out[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
            out[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
            out[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
            out[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
            out += 4;
        }
    }
}

void IMG_MipMap(byte *out, byte *in, int width, int height)
{
    int     i, j;

    width <<= 2;
    height >>= 1;
    for (i = 0; i < height; i++, in += width) {
        for (j = 0; j < width; j += 8, out += 4, in += 8) {
            out[0] = (in[0] + in[4] + in[width + 0] + in[width + 4]) >> 2;
            out[1] = (in[1] + in[5] + in[width + 1] + in[width + 5]) >> 2;
            out[2] = (in[2] + in[6] + in[width + 2] + in[width + 6]) >> 2;
            out[3] = (in[3] + in[7] + in[width + 3] + in[width + 7]) >> 2;
        }
    }
}

/*
=========================================================

IMAGE MANAGER

=========================================================
*/

#define RIMAGES_HASH    256

static list_t   r_imageHash[RIMAGES_HASH];

image_t     r_images[MAX_RIMAGES];
int         r_numImages;

uint32_t    d_8to24table[256];

static const struct {
    char    ext[4];
    int     (*load)(byte *, size_t, image_t *, byte **);
} img_loaders[IM_MAX] = {
    { "pcx", IMG_LoadPCX },
    { "wal", IMG_LoadWAL },
    { "tga", IMG_LoadSTB },
    { "jpg", IMG_LoadSTB },
    { "png", IMG_LoadSTB },
    { "dds", IMG_LoadDDS }
};

static imageformat_t    img_search[IM_MAX];
static int              img_total;

static cvar_t   *r_override_textures;
static cvar_t   *r_texture_formats;
static cvar_t   *r_texture_overrides;

static const cmd_option_t o_imagelist[] = {
    { "f", "fonts", "list fonts" },
    { "h", "help", "display this help message" },
    { "m", "skins", "list skins" },
    { "p", "pics", "list pics" },
    { "P", "placeholder", "list placeholder images" },
    { "s", "sprites", "list sprites" },
    { "w", "walls", "list walls" },
    { "y", "skies", "list skies" },
    { "S:string", "save", "save list to file"},
    { NULL }
};

static void IMG_List_c(genctx_t *ctx, int argnum)
{
    Cmd_Option_c(o_imagelist, NULL, ctx, argnum);
}

/*
===============
IMG_List_f
===============
*/
static void IMG_List_f(void)
{
    static const char types[8] = "PFMSWY??";
    image_t     *image;
    const char  *wildcard = NULL;
    bool        placeholder = false;
    int         i, c, mask = 0, count = 0;
    size_t      texels = 0;
    const char  *save_path = NULL;
    qhandle_t   f = 0;
    char        path[MAX_OSPATH];

    while ((c = Cmd_ParseOptions(o_imagelist)) != -1) {
        switch (c) {
        case 'p': mask |= 1 << IT_PIC;      break;
        case 'f': mask |= 1 << IT_FONT;     break;
        case 'm': mask |= 1 << IT_SKIN;     break;
        case 's': mask |= 1 << IT_SPRITE;   break;
        case 'w': mask |= 1 << IT_WALL;     break;
        case 'y': mask |= 1 << IT_SKY;      break;
        case 'P': placeholder = true;       break;
        case 'S': save_path = cmd_optarg;   break;
        case 'h':
            Cmd_PrintUsage(o_imagelist, "[wildcard]");
            Com_Printf("List registered images.\n");
            Cmd_PrintHelp(o_imagelist);
            Com_Printf(
                "Types legend:\n"
                "P: pics\n"
                "F: fonts\n"
                "M: skins\n"
                "S: sprites\n"
                "W: walls\n"
                "Y: skies\n"
                "\nFlags legend:\n"
                "T: transparent\n"
                "S: scrap\n"
                "*: permanent\n"
            );
            return;
        default:
            return;
        }
    }

    if (cmd_optind < Cmd_Argc())
        wildcard = Cmd_Argv(cmd_optind);

    if (save_path) {
        // save to file
        qhandle_t f = FS_EasyOpenFile(path, sizeof(path), FS_MODE_WRITE | FS_FLAG_TEXT, "", save_path, ".csv");
        if (!f) {
            Com_EPrintf("Error opening '%s'\n", path);
            return;
        }
    } else {
        Com_Printf("------------------\n");
    }

    for (i = 1, image = r_images + 1; i < r_numImages; i++, image++) {
        if (!image->registration_sequence)
            continue;
        if (mask && !(mask & (1 << image->type)))
            continue;
        if (wildcard && !Com_WildCmp(wildcard, image->name))
            continue;
        if ((image->width && image->height) == placeholder)
            continue;

        if (f) {
            char fmt[MAX_QPATH];
            sprintf(fmt, "%%-%ds, %%-%ds, (%% 5d %% 5d), sRGB:%%d\n", MAX_QPATH, MAX_QPATH);

            FS_FPrintf(f, fmt,
                image->name,
                image->filepath,
                image->width,
                image->height,
                image->is_srgb);
        } else {
            Com_Printf("%c%c%c%c %4i %4i %s: %s\n",
                    types[image->type > IT_MAX ? IT_MAX : image->type],
                    (image->flags & IF_TRANSPARENT) ? 'T' : ' ',
                    (image->flags & IF_SCRAP) ? 'S' : ' ',
                    (image->flags & IF_PERMANENT) ? '*' : ' ',
                    image->upload_width,
                    image->upload_height,
                    (image->flags & IF_PALETTED) ? "PAL" : "RGB",
                    image->name);
        }

        texels += image->upload_width * image->upload_height;
        count++;
    }

    if (f) {
        FS_CloseFile(f);
        Com_Printf("Saved '%s'\n", path);
    } else {
        Com_Printf("Total images: %d (out of %d slots)\n", count, r_numImages);
        Com_Printf("Total texels: %zu (not counting mipmaps)\n", texels);
    }
}

static image_t *alloc_image(void)
{
    int i;
    image_t *image, *placeholder = NULL;

    // find a free image_t slot
    for (i = 1, image = r_images + 1; i < r_numImages; i++, image++) {
        if (!image->registration_sequence)
            return image;
        if (!image->upload_width && !image->upload_height && !placeholder)
            placeholder = image;
    }

    // allocate new slot if possible
    if (r_numImages < MAX_RIMAGES) {
        r_numImages++;
        return image;
    }

    // reuse placeholder image if available
    if (placeholder) {
        List_Remove(&placeholder->entry);
        memset(placeholder, 0, sizeof(*placeholder));
        return placeholder;
    }

    return NULL;
}

// finds the given image of the given type.
// case and extension insensitive.
static image_t *lookup_image(const char *name,
                             imagetype_t type, unsigned hash, size_t baselen)
{
    image_t *image;

    // look for it
    LIST_FOR_EACH(image_t, image, &r_imageHash[hash], entry) {
        if (image->type != type) {
            continue;
        }
        if (image->baselen != baselen) {
            continue;
        }
        if (!FS_pathcmpn(image->name, name, baselen)) {
            return image;
        }
    }

    return NULL;
}

#define TRY_IMAGE_SRC_GAME      1
#define TRY_IMAGE_SRC_BASE      0

static int _try_image_format(imageformat_t fmt, image_t *image, int try_src, byte **pic)
{
    byte        *data;
    int         len;
    int         ret;

    // load the file
    int fs_flags = 0;
    if (try_src > 0)
        fs_flags = try_src == TRY_IMAGE_SRC_GAME ? FS_PATH_GAME : FS_PATH_BASE;
    len = FS_LoadFileFlags(image->name, (void **)&data, fs_flags);
    if (!data) {
        return len;
    }

    // decompress the image
    ret = img_loaders[fmt].load(data, len, image, pic);
    
    FS_FreeFile(data);

    image->filepath[0] = 0;
    if (ret >= 0) {
        strcpy(image->filepath, image->name);
        // record last modified time (skips reload when invoking IMG_ReloadAll)
        image->last_modified = 0;
        FS_LastModified(image->filepath, &image->last_modified);
    }
    return ret < 0 ? ret : fmt;
}

static int try_image_format(imageformat_t fmt, image_t *image, int try_src, byte **pic)
{
    // replace the extension
    memcpy(image->name + image->baselen + 1, img_loaders[fmt].ext, 4);
    return _try_image_format(fmt, image, try_src, pic);
}


// tries to load the image with a different extension
static int try_other_formats(imageformat_t orig, image_t *image, int try_src, byte **pic)
{
    imageformat_t   fmt;
    int             ret;
    int             i;

    // search through all the 32-bit formats
    for (i = 0; i < img_total; i++) {
        fmt = img_search[i];
        if (fmt == orig) {
            continue;   // don't retry twice
        }

        ret = try_image_format(fmt, image, try_src, pic);
        if (ret != Q_ERR(ENOENT)) {
            return ret; // found something
        }
    }

    // fall back to 8-bit formats
    fmt = (image->type == IT_WALL) ? IM_WAL : IM_PCX;
    if (fmt == orig) {
        return Q_ERR(ENOENT); // don't retry twice
    }

    return try_image_format(fmt, image, try_src, pic);
}

int IMG_GetDimensions(const char* name, int* width, int* height)
{
    assert(name);
    assert(width);
    assert(height);
    
    int w = 0;
    int h = 0;

    ssize_t len = strlen(name);
    if (len <= 4)
        return Q_ERR_NAMETOOSHORT;

    imageformat_t format;

    if (Q_stricmp(name + len - 4, ".wal") == 0)
        format = IM_WAL;
    else if (Q_stricmp(name + len - 4, ".pcx") == 0)
        format = IM_PCX;
    else if (Q_stricmp(name + len - 4, ".dds") == 0)
        format = IM_DDS;
    else
        return Q_ERR_INVALID_FORMAT;

    qhandle_t f;
    FS_OpenFile(name, &f, FS_MODE_READ | FS_FLAG_LOADFILE);
    if (!f)
        return Q_ERR(ENOENT);

    if (format == IM_WAL)
    {
        miptex_t mt;
        len = FS_Read(&mt, sizeof(mt), f);
        if (len == sizeof(mt)) {
            w = LittleLong(mt.width);
            h = LittleLong(mt.height);
        }
    }
    else if (format == IM_DDS)
    {
        DDS_HEADER dds = { 0 };
        len = FS_Read(&dds, sizeof(dds), f);
        if (len == sizeof(dds) && dds.magic == DDS_MAGIC && dds.size == sizeof(DDS_HEADER) - 4) {
            w = LittleLong(dds.width);
            h = LittleLong(dds.height);
        }
    }
    else if (format == IM_PCX)
    {
        dpcx_t pcx;
        len = FS_Read(&pcx, sizeof(pcx), f);
        if (len == sizeof(pcx)) {
            w = (LittleShort(pcx.xmax) - LittleShort(pcx.xmin)) + 1;
            h = (LittleShort(pcx.ymax) - LittleShort(pcx.ymin)) + 1;
        }
    }

    FS_CloseFile(f);

    if (w < 1 || h < 1 || w > MAX_TEXTURE_SIZE || h > MAX_TEXTURE_SIZE) {
        return Q_ERR_INVALID_FORMAT;
    }

    *width = w;
    *height = h;

    return Q_ERR_SUCCESS;
}

static void get_image_dimensions(imageformat_t fmt, image_t *image)
{
    char buffer[MAX_QPATH];
    memcpy(buffer, image->name, image->baselen + 1);
    memcpy(buffer + image->baselen + 1, img_loaders[fmt].ext, 4);

    IMG_GetDimensions(buffer, &image->width, &image->height);
}

static void r_texture_formats_changed(cvar_t *self)
{
    char *s;
    int i, j;

    // reset the search order
    img_total = 0;

    // parse the string
    for (s = self->string; *s; s++) {
        switch (*s) {
            case 'd': case 'D': i = IM_DDS; break;
            case 't': case 'T': i = IM_TGA; break;
            case 'j': case 'J': i = IM_JPG; break;
            case 'p': case 'P': i = IM_PNG; break;
            default: continue;
        }

        // don't let format to be specified more than once
        for (j = 0; j < img_total; j++)
            if (img_search[j] == i)
                break;
        if (j != img_total)
            continue;

        img_search[img_total++] = i;
        if (img_total == IM_MAX) {
            break;
        }
    }
}

int
load_img(const char *name, image_t *image)
{
    byte            *pic;
    imageformat_t   fmt;
    int             ret = Q_ERR(EINVAL);

	size_t len = strlen(name);

    // must have an extension and at least 1 char of base name
    if (len <= 4) {
        return Q_ERR_NAMETOOSHORT;
    }
    if (name[len - 4] != '.') {
        return Q_ERR_INVALID_PATH;
    }

    memcpy(image->name, name, len + 1);
    image->baselen = len - 4;
    image->type = 0;
    image->flags = 0;
    image->registration_sequence = 1;

    // find out original extension
    for (fmt = 0; fmt < IM_MAX; fmt++) {
        if (!Q_stricmp(image->name + image->baselen + 1, img_loaders[fmt].ext)) {
            break;
        }
    }

    // load the pic from disk
    pic = NULL;

    // Always prefer images from the game dir, even if format might be 'inferior'
    for (int try_location = Q_stricmp(fs_game->string, BASEGAME) ? TRY_IMAGE_SRC_GAME : TRY_IMAGE_SRC_BASE;
         try_location >= TRY_IMAGE_SRC_BASE;
         try_location--)
    {
        int location_flag = try_location == TRY_IMAGE_SRC_GAME ? IF_SRC_GAME : IF_SRC_MASK;
        if(((image->flags & IF_SRC_MASK) != 0) && ((image->flags & IF_SRC_MASK) != location_flag))
            continue;

        // first try with original extension
        ret = _try_image_format(fmt, image, try_location, &pic);
        if (ret == Q_ERR(ENOENT)) {
            // retry with remaining extensions
            ret = try_other_formats(fmt, image, try_location, &pic);
        }
        if (ret >= 0)
            break;
    }

    // if we are replacing 8-bit texture with a higher resolution 32-bit
    // texture, we need to recover original image dimensions
    if (fmt <= IM_WAL && ret > IM_WAL) {
        get_image_dimensions(fmt, image);
    }

    if (ret < 0) {
        memset(image, 0, sizeof(*image));
        return ret;
    }

#if USE_REF == REF_VKPT
	image->pix_data = pic;
#endif

    return Q_ERR_SUCCESS;
}

static bool need_override_image(imagetype_t type, imageformat_t fmt)
{
    if (r_override_textures->integer < 1)
        return false;
    if (r_override_textures->integer == 1 && fmt > IM_WAL)
        return false;
    return r_texture_overrides->integer & (1 << type);
}

// Try to load an image, possibly with an alternative extension
static int try_load_image_candidate(image_t *image, const char *orig_name, size_t orig_len, byte **pic_p, imagetype_t type, imageflags_t flags, bool allow_override, int try_location)
{
    int ret;

    image->type = type;
    image->flags = flags;
    image->registration_sequence = registration_sequence;

    // find out original extension
    imageformat_t fmt;
    for (fmt = 0; fmt < IM_MAX; fmt++)
    {
        if (!Q_stricmp(image->name + image->baselen + 1, img_loaders[fmt].ext))
        {
            break;
        }
    }

    bool override_texture = !allow_override || (flags & IF_EXACT) ? false : need_override_image(type, fmt);

    // load the pic from disk
    *pic_p = NULL;

    if (fmt == IM_MAX)
    {
        // unknown extension, but give it a chance to load anyway
        ret = try_other_formats(IM_MAX, image, try_location, pic_p);
        if (ret == Q_ERR(ENOENT))
        {
            // not found, change error to invalid path
            ret = Q_ERR_INVALID_PATH;
        }
    }
    else if (override_texture)
    {
        // forcibly replace the extension
        ret = try_other_formats(IM_MAX, image, try_location, pic_p);
    }
    else
    {
        // first try with original extension
        ret = _try_image_format(fmt, image, try_location, pic_p);
        if (ret == Q_ERR(ENOENT) && !(flags & IF_EXACT))
        {
            // retry with remaining extensions
            ret = try_other_formats(fmt, image, try_location, pic_p);
        }
    }

    // record last modified time (skips reload when invoking IMG_ReloadAll)
    image->last_modified = 0;
    FS_LastModified(image->name, &image->last_modified);

    // Restore original name if it was overridden
    if(orig_name) {
        memcpy(image->name, orig_name, orig_len + 1);
        image->baselen = orig_len - 4;
    }

    // if we are replacing 8-bit texture with a higher resolution 32-bit
    // texture, we need to recover original image dimensions
    if (fmt <= IM_WAL && ret > IM_WAL)
    {
        get_image_dimensions(fmt, image);
    }

    return ret;
}

static void print_error(const char *name, imageflags_t flags, int err)
{
    const char *msg;
    int level = PRINT_ERROR;

    switch (err) {
    case Q_ERR_INVALID_FORMAT:
    case Q_ERR_LIBRARY_ERROR:
        msg = Com_GetLastError();
        break;
    case Q_ERR(ENOENT):
        if (flags & IF_PERMANENT) {
            // ugly hack for console code
            if (strcmp(name, "pics/conchars.pcx"))
                level = PRINT_WARNING;
#if USE_DEBUG
        } else if (developer->integer >= 2) {
            level = PRINT_DEVELOPER;
#endif
        } else {
            return;
        }
        // fall through
    default:
        msg = Q_ErrorString(err);
        break;
    }

    Com_LPrintf(level, "Couldn't load %s: %s\n", name, msg);
}

// finds or loads the given image, adding it to the hash table.
static image_t *find_or_load_image(const char *name, size_t len,
                                   imagetype_t type, imageflags_t flags)
{
    image_t         *image;
    byte            *pic;
    unsigned        hash;
    int             ret = Q_ERR(ENOENT);

    // must have an extension and at least 1 char of base name
    if (len <= 4) {
        ret = Q_ERR_NAMETOOSHORT;
        goto fail;
    }
    if (name[len - 4] != '.') {
        ret = Q_ERR_INVALID_PATH;
        goto fail;
    }

    hash = FS_HashPathLen(name, len - 4, RIMAGES_HASH);

    // look for it
    if ((image = lookup_image(name, type, hash, len - 4)) != NULL) {
        image->registration_sequence = registration_sequence;
        if (image->upload_width && image->upload_height) {
            image->flags |= flags & IF_PERMANENT;
            return image;
        }
        return NULL;
    }

    // allocate image slot
    image = alloc_image();
    if (!image) {
        ret = Q_ERR_OUT_OF_SLOTS;
        goto fail;
    }

    bool allow_override = cls.ref_type != REF_TYPE_GL || type == IT_PIC || gl_use_hd_assets->integer;

    if(allow_override)
    {
        const char *last_slash = strrchr(name, '/');
        if (!last_slash)
            last_slash = name;
        else
            last_slash += 1;

        strcpy(image->name, "overrides/");
        strcat(image->name, last_slash);
        image->baselen = strlen(image->name) - 4;
        ret = try_load_image_candidate(image, name, len, &pic, type, flags, true, -1);
        memcpy(image->name, name, len + 1);
        image->baselen = len - 4;
    }

    // Try non-overridden image
    if (ret < 0)
    {
        bool is_not_baseq2 = fs_game->string[0] && strcmp(fs_game->string, BASEGAME) != 0;
    	
        // Always prefer images from the game dir, even if format might be 'inferior'
        for (int try_location = is_not_baseq2 ? TRY_IMAGE_SRC_GAME : TRY_IMAGE_SRC_BASE;
            try_location >= TRY_IMAGE_SRC_BASE;
            try_location--)
        {
            int location_flag = try_location == TRY_IMAGE_SRC_GAME ? IF_SRC_GAME : IF_SRC_BASE;
            if(((flags & IF_SRC_MASK) != 0) && ((flags & IF_SRC_MASK) != location_flag))
                continue;

            // fill in some basic info
            memcpy(image->name, name, len + 1);
            image->baselen = len - 4;
            ret = try_load_image_candidate(image, NULL, 0, &pic, type, flags, !!allow_override, try_location);
            image->flags |= location_flag;

            if (ret >= 0)
                break;
        }
    }

    if (ret < 0) {
        print_error(image->name, flags, ret);
        if (flags & IF_PERMANENT) {
            memset(image, 0, sizeof(*image));
        } else {
            // don't reload temp pics every frame
            image->upload_width = image->upload_height = 0;
            List_Append(&r_imageHash[hash], &image->entry);
        }
        return NULL;
    }

    image->aspect = (float)image->upload_width / image->upload_height;

    List_Append(&r_imageHash[hash], &image->entry);

	image->is_srgb = !!(flags & IF_SRGB);

    // upload the image
    IMG_Load(image, pic);

    return image;

fail:
    print_error(name, flags, ret);
    return NULL;
}

image_t *IMG_Find(const char *name, imagetype_t type, imageflags_t flags)
{
    image_t *image;
    size_t len;

    Q_assert(name);

    len = strlen(name);
    Q_assert(len < MAX_QPATH);

    if ((image = find_or_load_image(name, len, type, flags))) {
        return image;
    }
    return R_NOTEXTURE;
}

image_t *IMG_FindExisting(const char *name, imagetype_t type)
{
    image_t *image;
    size_t len;
    unsigned hash;

    if (!name) {
        Com_Error(ERR_FATAL, "%s: NULL", __func__);
        return R_NOTEXTURE;
    }

    // this should never happen
    len = strlen(name);
    if (len >= MAX_QPATH) {
        Com_Error(ERR_FATAL, "%s: oversize name", __func__);
        return R_NOTEXTURE;
    }

    // must have an extension and at least 1 char of base name
    if (len <= 4) {
        return R_NOTEXTURE;
    }
    if (len > 4 && name && name[len - 4] != '.') {
        return R_NOTEXTURE;
    }

    hash = FS_HashPathLen(name, len - 4, RIMAGES_HASH);

    // look for it
    if ((image = lookup_image(name, type, hash, len - 4)) != NULL) {
        return image;
    }

    return R_NOTEXTURE;
}

/*
===============
IMG_Clone
===============
*/
image_t *IMG_Clone(image_t *image, const char* new_name)
{
    if(image == R_NOTEXTURE)
        return image;

    image_t* new_image = alloc_image();
    if (!new_image)
        return R_NOTEXTURE;

    memcpy(new_image, image, sizeof(image_t));

#if USE_REF == REF_VKPT
    size_t image_size = image->pix_data_size;
    if(image->pix_data != NULL && image_size > 0)
    {
        new_image->pix_data = IMG_AllocPixels(image_size);
        memcpy(new_image->pix_data, image->pix_data, image_size);
    }
#else
    for (int m = 0; m < 4; m++)
    {
        if(image->pixels[m] != NULL)
        {
            size_t mip_size = (image->upload_width >> m) * (image->upload_height >> m) * 4;
            new_image->pixels[m] = IMG_AllocPixels(mip_size);
            memcpy(new_image->pixels[m], image->pixels[m], mip_size);
        }
    }
#endif

    if(new_name)
    {
        Q_strlcpy(new_image->name, new_name, sizeof(new_image->name));
        new_image->baselen = strlen(new_image->name) - 4;
        assert(new_image->name[new_image->baselen] == '.');
    }
    unsigned hash = FS_HashPathLen(new_image->name, new_image->baselen, RIMAGES_HASH);
    List_Append(&r_imageHash[hash], &new_image->entry);
    return new_image;
}

/*
===============
IMG_ForHandle
===============
*/
image_t *IMG_ForHandle(qhandle_t h)
{
    Q_assert(h >= 0 && h < r_numImages);
    return &r_images[h];
}

/*
===============
R_RegisterImage
===============
*/
qhandle_t R_RegisterImage(const char *name, imagetype_t type, imageflags_t flags)
{
    image_t     *image;
    char        fullname[MAX_QPATH];
    size_t      len;

    // empty names are legal, silently ignore them
    if (!*name) {
        return 0;
    }

    // no images = not initialized
    if (!r_numImages) {
        return 0;
    }

    if (type == IT_SKIN || *name == '/' || *name == '\\' || !strncmp(name, "textures/", 9) || !strncmp(name, "models/", 7) || !strncmp(name, "pics/", 5) || !strncmp(name, "env/", 4) || !strncmp(name, "fonts/", 6)) {
        len = FS_NormalizePathBuffer(fullname, (*name == '/' || *name == '\\') ? name + 1 : name, sizeof(fullname));
    } else {
        len = Q_concat(fullname, sizeof(fullname), "pics/", name);
        if (len < sizeof(fullname)) {
            FS_NormalizePath(fullname);
            len = COM_DefaultExtension(fullname, ".pcx", sizeof(fullname));
        }
    }

    if (len >= sizeof(fullname)) {
        print_error(fullname, flags, Q_ERR(ENAMETOOLONG));
        return 0;
    }

    if ((image = find_or_load_image(fullname, len, type, flags))) {
        return image - r_images;
    }
    return 0;
}

qhandle_t R_RegisterRawImage(const char *name, int width, int height, byte* pic, imagetype_t type, imageflags_t flags)
{
    image_t         *image;
    unsigned        hash;

    int len = strlen(name);
    hash = FS_HashPathLen(name, len, RIMAGES_HASH);

    // look for it
    if ((image = lookup_image(name, type, hash, len)) != NULL) {
        image->flags |= flags & IF_PERMANENT;
        image->registration_sequence = registration_sequence;
#if USE_REF == REF_VKPT
        image->skip_runtime_normalization = false;
#endif
        return image - r_images;
    }

    // allocate image slot
    image = alloc_image();
    if (!image) {
        return 0;
    }

    memcpy(image->name, name, len + 1);
    image->baselen = len;
    image->type = type;
    image->flags = flags;
    image->registration_sequence = registration_sequence;
    image->last_modified = 0;
    image->width = width;
    image->height = height;
    image->upload_width = width;
    image->upload_height = height;
    image->pixel_format = PF_R8G8B8A8_UNORM;
    image->pix_data_size = (size_t)width * (size_t)height * 4u;
    image->mip_levels = 0;
#if USE_REF == REF_VKPT
    image->skip_runtime_normalization = false;
#endif

    List_Append(&r_imageHash[hash], &image->entry);

    image->is_srgb = !!(flags & IF_SRGB);

    // upload the image
    IMG_Load(image, pic);

    return image - r_images;
}

void R_UnregisterImage(qhandle_t handle)
{
    if (!handle)
        return;

    image_t* image = r_images + handle;

    if (image->registration_sequence)
    {
        image->registration_sequence = -1;
        IMG_FreeUnused();
    }
}

/*
=============
R_GetPicSize
=============
*/
bool R_GetPicSize(int *w, int *h, qhandle_t pic)
{
    image_t *image = IMG_ForHandle(pic);

    if (w) {
        *w = image->width;
    }
    if (h) {
        *h = image->height;
    }
    return image->flags & IF_TRANSPARENT;
}

/*
================
IMG_FreeUnused

Any image that was not touched on this registration sequence
will be freed.
================
*/
void IMG_FreeUnused(void)
{
    image_t *image;
    int i, count = 0;

    for (i = 1, image = r_images + 1; i < r_numImages; i++, image++) {
		if ( !image ) {
			continue;
		}
		if (image->registration_sequence == registration_sequence) {
            continue;        // used this sequence
        }
        if (!image->registration_sequence)
            continue;        // free image_t slot
        if (image->flags & (IF_PERMANENT | IF_SCRAP))
            continue;        // don't free pics

        // delete it from hash table
        List_Remove(&image->entry);

        // free it
        IMG_Unload(image);

        memset(image, 0, sizeof(*image));
        count++;
    }

    if (count) {
        Com_DPrintf("%s: %i images freed\n", __func__, count);
    }
}

void IMG_FreeAll(void)
{
    image_t *image;
    int i, count = 0;

    for (i = 1, image = r_images + 1; i < r_numImages; i++, image++) {
        if (!image->registration_sequence)
            continue;        // free image_t slot
        // free it
        IMG_Unload(image);

        memset(image, 0, sizeof(*image));
        count++;
    }

    if (count) {
        Com_DPrintf("%s: %i images freed\n", __func__, count);
    }

    for (i = 0; i < RIMAGES_HASH; i++) {
        List_Init(&r_imageHash[i]);
    }

    // &r_images[0] == R_NOTEXTURE
    r_numImages = 1;
}

/*
===============
R_GetPalette

===============
*/
void IMG_GetPalette(void)
{
    byte        pal[768], *src, *data;
    int         i, ret, len;

    // get the palette
    len = FS_LoadFile(R_COLORMAP_PCX, (void **)&data);
    if (!data) {
        ret = len;
        goto fail;
    }

    ret = IMG_DecodePCX(data, len, NULL, pal, NULL, NULL);

    FS_FreeFile(data);

    if (ret < 0) {
        goto fail;
    }

    for (i = 0, src = pal; i < 255; i++, src += 3) {
        d_8to24table[i] = MakeColor(src[0], src[1], src[2], 255);
    }

    // 255 is transparent
    d_8to24table[i] = MakeColor(src[0], src[1], src[2], 0);
    return;

fail:
    Com_Error(ERR_FATAL, "Couldn't load %s: %s", R_COLORMAP_PCX, Q_ErrorString(ret));
}

static const cmdreg_t img_cmd[] = {
    { "imagelist", IMG_List_f, IMG_List_c },
    { "screenshot", IMG_ScreenShot_f },
    { "screenshottga", IMG_ScreenShotTGA_f },
    { "screenshotjpg", IMG_ScreenShotJPG_f },
    { "screenshotpng", IMG_ScreenShotPNG_f },
    { "screenshothdr", IMG_ScreenShotHDR_f },
    { NULL }
};

void IMG_Init(void)
{
    int i;

    Q_assert(!r_numImages);

    r_override_textures = Cvar_Get("r_override_textures", "1", CVAR_FILES);
    r_texture_formats = Cvar_Get("r_texture_formats", "dtpj", 0);
    r_texture_formats->changed = r_texture_formats_changed;
    r_texture_formats_changed(r_texture_formats);
    r_texture_overrides = Cvar_Get("r_texture_overrides", "-1", CVAR_FILES);

    r_screenshot_format = Cvar_Get("gl_screenshot_format", "png", CVAR_ARCHIVE);
    r_screenshot_async = Cvar_Get("gl_screenshot_async", "1", 0);
    r_screenshot_quality = Cvar_Get("gl_screenshot_quality", "100", CVAR_ARCHIVE);
    r_screenshot_compression = Cvar_Get("gl_screenshot_compression", "6", CVAR_ARCHIVE);
    r_screenshot_message = Cvar_Get("gl_screenshot_message", "0", CVAR_ARCHIVE);
    r_screenshot_template = Cvar_Get("gl_screenshot_template", "quakeXXX", 0);

    Cmd_Register(img_cmd);

    for (i = 0; i < RIMAGES_HASH; i++) {
        List_Init(&r_imageHash[i]);
    }

    // &r_images[0] == R_NOTEXTURE
    r_numImages = 1;
}

void IMG_Shutdown(void)
{
    Cmd_Deregister(img_cmd);
    r_numImages = 0;
}
