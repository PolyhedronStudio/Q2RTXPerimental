/********************************************************************
*
*
*	Refresh: Alembic (.abc) Ogawa Model Format Definitions
*
*
********************************************************************/
#pragma once



/**
*
*
*	.abc - Alembic (Ogawa Container) Model Format Constants:
*
*
**/
//! FourCC magic identifier for Alembic Ogawa archives ("Ogaw" from the 5-byte "Ogawa" header).
#define ABC_IDENT					MakeLittleLong( 'O', 'g', 'a', 'w' )
//! Expected 5th byte of the "Ogawa" magic signature.
#define ABC_MAGIC_BYTE4				'a'
//! Expected Ogawa binary container version (big-endian 0x0001 at bytes 6..7).
#define ABC_OGAWA_VERSION			1

//! Maximum supported animation frames per Alembic model.
#define ABC_MAX_FRAMES				1024
//! Maximum supported IPolyMesh objects (submeshes) per Alembic model.
#define ABC_MAX_MESHES				32
//! Maximum supported vertices per frame in a single Alembic submesh.
#define ABC_MAX_VERTS_PER_FRAME		524288
//! Maximum supported triangles per frame in a single Alembic submesh.
#define ABC_MAX_TRIS_PER_FRAME		262144
