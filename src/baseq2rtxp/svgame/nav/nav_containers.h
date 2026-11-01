/********************************************************************
*
*
*	ServerGame: Navigation Spatial Containers
*
*	Spatial grid and spatial indexing structures for navigation queries.
*	(Custom nav_vector_t removed in Milestone M2 in favor of svg_vector_t).
*
*
********************************************************************/
#pragma once

#include "nav_core.h"
#include <utility>
#include <vector>
#include <unordered_map>
#include <cmath>

/**
*	@brief	Generic 3D spatial hash grid container for fast spatial proximity queries.
*	@tparam	TItem	Type of identifier or data stored in grid cells.
**/
template <typename TItem>
class nav_spatial_grid_t {
private:
	//! Grid cell spatial extents (in Quake units).
	double cell_size = 128.0;
	//! Backing map mapping 64-bit spatial cell key to item vectors.
	std::unordered_map<uint64_t, std::vector<TItem>> grid;

	/**
	*	@brief	Compute 64-bit hash key from 3D integer cell coordinates.
	**/
	inline uint64_t HashCellKey( int64_t cx, int64_t cy, int64_t cz ) const {
		return ( static_cast<uint64_t>( cx ) * 73856093ULL ) ^
			( static_cast<uint64_t>( cy ) * 19349663ULL ) ^
			( static_cast<uint64_t>( cz ) * 83492791ULL );
	}

public:
	nav_spatial_grid_t() = default;
	explicit nav_spatial_grid_t( double in_cell_size ) : cell_size( in_cell_size ) {}

	/**
	*	@brief	Reset grid contents and set cell dimension.
	**/
	void Initialize( double in_cell_size ) {
		cell_size = in_cell_size;
		grid.clear();
	}

	/**
	*	@brief	Clear all spatial items from grid cells.
	**/
	void Clear() {
		grid.clear();
	}

	/**
	*	@brief	Insert an item at a specific 3D point location.
	**/
	void InsertPoint( const Vector3DP &point, const TItem &item ) {
		const int64_t cx = static_cast<int64_t>( std::floor( point.x / cell_size ) );
		const int64_t cy = static_cast<int64_t>( std::floor( point.y / cell_size ) );
		const int64_t cz = static_cast<int64_t>( std::floor( point.z / cell_size ) );
		grid[ HashCellKey( cx, cy, cz ) ].push_back( item );
	}

	/**
	*	@brief	Insert an item into all grid cells overlapping an AABB range.
	**/
	void InsertBox( const Vector3DP &mins, const Vector3DP &maxs, const TItem &item ) {
		const int64_t min_x = static_cast<int64_t>( std::floor( mins.x / cell_size ) );
		const int64_t min_y = static_cast<int64_t>( std::floor( mins.y / cell_size ) );
		const int64_t min_z = static_cast<int64_t>( std::floor( mins.z / cell_size ) );

		const int64_t max_x = static_cast<int64_t>( std::floor( maxs.x / cell_size ) );
		const int64_t max_y = static_cast<int64_t>( std::floor( maxs.y / cell_size ) );
		const int64_t max_z = static_cast<int64_t>( std::floor( maxs.z / cell_size ) );

		for ( int64_t cx = min_x; cx <= max_x; ++cx ) {
			for ( int64_t cy = min_y; cy <= max_y; ++cy ) {
				for ( int64_t cz = min_z; cz <= max_z; ++cz ) {
					grid[ HashCellKey( cx, cy, cz ) ].push_back( item );
				}
			}
		}
	}

	/**
	*	@brief	Execute a callback for all items located in cells overlapping an AABB range.
	*	@tparam	Visitor	Lambda or function object accepting (const TItem &).
	**/
	template <typename Visitor>
	void QueryBox( const Vector3DP &mins, const Vector3DP &maxs, Visitor &&visitor ) const {
		const int64_t min_x = static_cast<int64_t>( std::floor( mins.x / cell_size ) );
		const int64_t min_y = static_cast<int64_t>( std::floor( mins.y / cell_size ) );
		const int64_t min_z = static_cast<int64_t>( std::floor( mins.z / cell_size ) );

		const int64_t max_x = static_cast<int64_t>( std::floor( maxs.x / cell_size ) );
		const int64_t max_y = static_cast<int64_t>( std::floor( maxs.y / cell_size ) );
		const int64_t max_z = static_cast<int64_t>( std::floor( maxs.z / cell_size ) );

		for ( int64_t cx = min_x; cx <= max_x; ++cx ) {
			for ( int64_t cy = min_y; cy <= max_y; ++cy ) {
				for ( int64_t cz = min_z; cz <= max_z; ++cz ) {
					auto it = grid.find( HashCellKey( cx, cy, cz ) );
					if ( it != grid.end() ) {
						for ( const auto &item : it->second ) {
							visitor( item );
						}
					}
				}
			}
		}
	}
};

