/*
Copyright (C) 2022 Andrey Nazarov

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

#ifndef HUFFMAN_H
#define HUFFMAN_H



/* This is based on the Adaptive Huffman algorithm described in Sayood's Data
 * Compression book.  The ranks are not actually stored, but implicitly defined
 * by the location of a node within a doubly-linked list */
#define NYT HMAX					/* NYT = Not Yet Transmitted */
#define INTERNAL_NODE (HMAX+1)

typedef struct nodetype {
	struct	nodetype *left, *right, *parent; /* tree structure */
	struct	nodetype *next, *prev; /* doubly-linked list */
	struct	nodetype **head; /* highest ranked node in block */
	int		weight;
	int		symbol;
} node_t;

#define HMAX 256 /* Maximum symbol */

typedef struct {
	int			blocNode;
	int			blocPtrs;

	node_t *tree;
	node_t *lhead;
	node_t *ltail;
	node_t *loc[ HMAX + 1 ];
	node_t **freelist;

	node_t		nodeList[ 768 ];
	node_t *nodePtrs[ 768 ];
} huff_t;

typedef struct {
	huff_t		compressor;
	huff_t		decompressor;
} huffman_t;

void	Huff_Compress( struct sizebuf_s *buf, int offset );
void	Huff_Decompress( struct sizebuf_s *buf, int offset );
void	Huff_Init( huffman_t *huff );
void	Huff_addRef( huff_t *huff, byte ch );
int		Huff_Receive( node_t *node, int *ch, byte *fin );
void	Huff_transmit( huff_t *huff, int ch, byte *fout, int maxoffset );
void	Huff_offsetReceive( node_t *node, int *ch, byte *fin, int *offset, int maxoffset );
void	Huff_offsetTransmit( huff_t *huff, int ch, byte *fout, int *offset, int maxoffset );
void	Huff_putBit( int bit, byte *fout, int *offset );
int		Huff_getBit( byte *fout, int *offset );

// don't use if you don't know what you're doing.
int		Huff_getBloc( void );
void	Huff_setBloc( int _bloc );


extern huffman_t clientHuffTables;

#define	SV_ENCODE_START		4
#define SV_DECODE_START		12
#define	CL_ENCODE_START		12
#define CL_DECODE_START		4



#endif // #define HUFFMAN_H