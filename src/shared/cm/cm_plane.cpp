/********************************************************************
*
*
*	Shared: Unique BSP Plane Key Implementation:
*
*
********************************************************************/
#include "shared/shared.h"
#if 0
namespace plane_key {

	static inline int64_t llabs64( int64_t v ) { return v < 0 ? -v : v; }

	static int64_t gcd64( int64_t a, int64_t b ) {
		if ( a == 0 ) return llabs64( b );
		if ( b == 0 ) return llabs64( a );
		a = llabs64( a ); b = llabs64( b );
		while ( b ) { int64_t r = a % b; a = b; b = r; }
		return a;
	}

	static int64_t gcd4( int64_t a, int64_t b, int64_t c, int64_t d ) {
		int64_t g = gcd64( a, b );
		g = gcd64( g, c );
		g = gcd64( g, d );
		return g;
	}

	std::string PlaneKey::toString() const {
		std::ostringstream ss;
		ss << "(" << a << "," << b << "," << c << "," << d << ")";
		return ss.str();
	}

	static IVec3 snapToGrid( const std::array<float, 3> &v, float gridRes ) {
		float inv = 1.0f / gridRes;
		// Use llround to reduce rounding surprises across platforms
		IVec3 r;
		r.x = static_cast< int64_t >( std::llround( v[ 0 ] * inv ) );
		r.y = static_cast< int64_t >( std::llround( v[ 1 ] * inv ) );
		r.z = static_cast< int64_t >( std::llround( v[ 2 ] * inv ) );
		return r;
	}

	static IVec3 sub( const IVec3 &a, const IVec3 &b ) {
		return { a.x - b.x, a.y - b.y, a.z - b.z };
	}

	static IVec3 cross( const IVec3 &u, const IVec3 &v ) {
		// Cross product with 64-bit integers; watch for overflow if coordinates are huge.
		return {
			u.y * v.z - u.z * v.y,
			u.z * v.x - u.x * v.z,
			u.x * v.y - u.y * v.x
		};
	}

	static int64_t dot( const IVec3 &u, const IVec3 &v ) {
		return u.x * v.x + u.y * v.y + u.z * v.z;
	}

	static bool isZeroVec( const IVec3 &v ) {
		return v.x == 0 && v.y == 0 && v.z == 0;
	}

	// Reduce (a,b,c,d) by gcd and enforce deterministic sign convention:
	// - Find first non-zero component of (a,b,c) in order a,b,c.
	// - If that component < 0, multiply all by -1.
	static void reduce_and_canonicalize( int64_t &a, int64_t &b, int64_t &c, int64_t &d ) {
		int64_t g = gcd4( a, b, c, d );
		if ( g > 1 ) {
			a /= g; b /= g; c /= g; d /= g;
		}
		// deterministic sign: first non-zero of (a,b,c) must be positive
		if ( a < 0 || ( a == 0 && b < 0 ) || ( a == 0 && b == 0 && c < 0 ) ) {
			a = -a; b = -b; c = -c; d = -d;
		}
	}

	// Primary API: build key from three non-collinear snapped vertices found in verts
	PlaneKey fromFaceVertices( const std::vector<std::array<float, 3>> &verts, float gridRes ) {
		if ( verts.size() < 3 ) throw std::invalid_argument( "fromFaceVertices requires >=3 vertices" );

		// Snap all verts to grid
		std::vector<IVec3> sverts;
		sverts.reserve( verts.size() );
		for ( const auto &v : verts ) sverts.push_back( snapToGrid( v, gridRes ) );

		// Find first triple of non-collinear snapped points
		size_t n = sverts.size();
		for ( size_t i = 0; i < n; ++i ) {
			for ( size_t j = i + 1; j < n; ++j ) {
				for ( size_t k = j + 1; k < n; ++k ) {
					IVec3 v1 = sub( sverts[ j ], sverts[ i ] );
					IVec3 v2 = sub( sverts[ k ], sverts[ i ] );
					IVec3 normal = cross( v1, v2 );
					if ( !isZeroVec( normal ) ) {
						// compute D = dot(normal, p0)
						int64_t D = dot( normal, sverts[ i ] );
						int64_t a = normal.x, b = normal.y, c = normal.z;
						reduce_and_canonicalize( a, b, c, D );
						return PlaneKey{ a,b,c,D };
					}
				}
			}
		}

		throw std::runtime_error( "All snapped vertices are collinear or identical; cannot form plane key" );
	}

	// Fallback: attempt to form a key from a cplane-like structure (origin + dist).
	// This function quantizes the plane normal and D to integers. It is useful as a
	// best-effort fallback, but WILL NOT be as robust/unique as using snapped mesh vertices.
	// Use only if you cannot access the vertices; prefer fromFaceVertices whenever possible.
	PlaneKey fromCPlane( const cplane_like &cp, float gridRes ) {
		// Interpret cp.origin[] as a point on the plane and cp.dist as plane distance in same units.
		// We'll snap the origin to the grid and quantize the normal to a rational approximation
		// tied to the grid factor. This is deterministic but not mathematically exact for arbitrary floats.
		const int64_t gridFactor = static_cast< int64_t >( std::llround( 1.0 / gridRes ) );
		// Normalize normal (defensive)
		float nx = 0, ny = 0, nz = 0;
		// We don't have the normal explicitly by name here (user said cplane_t has origin+dist),
		// but if your actual cplane_t stores a normal vector instead of origin, change here.
		// For this fallback we assume cp.origin is a point and cp.dist is not usable to compute
		// a normal, so we can't produce a robust key. To keep a deterministic function, we quantize
		// origin only and set a key derived from origin plane projection along axes.
		//
		// NOTE: This fallback is intentionally simple; real robustness requires vertices.
		IVec3 p0 = snapToGrid( { cp.origin[ 0 ], cp.origin[ 1 ], cp.origin[ 2 ] }, gridRes );

		// As a deterministic fallback: project the plane onto axes and make a canonical key.
		// This only provides stability, not full mathematical uniqueness for all planes.
		// Use high-entropy combination: pack p0 and dist scaled by gridFactor.
		int64_t D = static_cast< int64_t >( std::llround( cp.dist * gridFactor ) );
		int64_t a = p0.x;
		int64_t b = p0.y;
		int64_t c = p0.z;
		reduce_and_canonicalize( a, b, c, D );
		return PlaneKey{ a,b,c,D };
	}

} // namespace plane_key
#endif