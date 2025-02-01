/********************************************************************
*
*
*	SharedGame: time type.
*
*
********************************************************************/
// Credits to Paril from KEX/Q2RE.
#pragma once

// stores a level time; most newer engines use int64_t for
// time storage, but seconds are handy for compatibility
// with Quake and older mods.
struct QMTime {
private:
	// times always start at zero, just to prevent memory issues
	int64_t _ms = 0;

	// internal; use _sec/_ms/_min or QMTime::FromMilliseconds(n)/QMTime::FromMilliseconds((n)/QMTime::from_min(n)
	constexpr explicit QMTime( const int64_t &ms ) : _ms( ms ) {
	}

public:
	constexpr QMTime( ) = default;
	constexpr QMTime( const QMTime & ) = default;
	constexpr QMTime &operator=( const QMTime & ) = default;

	// constructors are here, explicitly named, so you always
	// know what you're getting.

	// new time from ms
	static constexpr QMTime from_ms( const int64_t &ms ) {
		return QMTime( ms );
	}

	// new time from seconds
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	static constexpr QMTime from_sec( const T &s ) {
		return QMTime( static_cast<int64_t>( s * 1000 ) );
	}

	// new time from minutes
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	static constexpr QMTime from_min( const T &s ) {
		return QMTime( static_cast<int64_t>( s * 60000 ) );
	}

	// new time from hz
	static constexpr QMTime from_hz( const uint64_t &hz ) {
		return from_ms( static_cast<int64_t>( ( 1.0 / hz ) * 1000 ) );
	}

	// get value in minutes (truncated for integers)
	template<typename T = float>
	constexpr T minutes( ) const {
		return static_cast<T>( _ms / static_cast<std::conditional_t<std::is_floating_point_v<T>, T, float>>( 60000 ) );
	}

	// get value in seconds (truncated for integers)
	template<typename T = float>
	constexpr T seconds( ) const {
		return static_cast<T>( _ms / static_cast<std::conditional_t<std::is_floating_point_v<T>, T, float>>( 1000 ) );
	}

	// get value in milliseconds
	constexpr const int64_t &milliseconds( ) const {
		return _ms;
	}

	int64_t frames( ) const;;

	// check if non-zero
	constexpr explicit operator bool( ) const {
		return !!_ms;
	}

	// invert time
	constexpr QMTime operator-( ) const {
		return QMTime( -_ms );
	}

	// operations with other times as input
	constexpr QMTime operator+( const QMTime &r ) const {
		return QMTime( _ms + r._ms );
	}
	constexpr QMTime operator-( const QMTime &r ) const {
		return QMTime( _ms - r._ms );
	}
	constexpr QMTime &operator+=( const QMTime &r ) {
		return *this = *this + r;
	}
	constexpr QMTime &operator-=( const QMTime &r ) {
		return *this = *this - r;
	}

	// operations with scalars as input
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr QMTime operator*( const T &r ) const {
		return QMTime::FromMilliseconds( static_cast<int64_t>( _ms * r ) );
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr QMTime operator/( const T &r ) const {
		return QMTime::FromMilliseconds( static_cast<int64_t>( _ms / r ) );
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr QMTime &operator*=( const T &r ) {
		return *this = *this * r;
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr QMTime &operator/=( const T &r ) {
		return *this = *this / r;
	}

	// comparisons with gtime_ts
	constexpr bool operator==( const QMTime &time ) const {
		return _ms == time._ms;
	}
	constexpr bool operator!=( const QMTime &time ) const {
		return _ms != time._ms;
	}
	constexpr bool operator<( const QMTime &time ) const {
		return _ms < time._ms;
	}
	constexpr bool operator>( const QMTime &time ) const {
		return _ms > time._ms;
	}
	constexpr bool operator<=( const QMTime &time ) const {
		return _ms <= time._ms;
	}
	constexpr bool operator>=( const QMTime &time ) const {
		return _ms >= time._ms;
	}
};

// user literals, allowing you to specify times
// as 128_sec and 128_ms
constexpr QMTime operator"" _min( long double s ) {
	return QMTime::from_min( s );
}
constexpr QMTime operator"" _min( unsigned long long int s ) {
	return QMTime::from_min( s );
}
constexpr QMTime operator"" _sec( long double s ) {
	return QMTime::FromMilliseconds( s );
}
constexpr QMTime operator"" _sec( unsigned long long int s ) {
	return QMTime::FromMilliseconds( s );
}
constexpr QMTime operator"" _ms( long double s ) {
	return QMTime::FromMilliseconds( static_cast<int64_t>( s ) );
}
constexpr QMTime operator"" _ms( unsigned long long int s ) {
	return QMTime::FromMilliseconds( static_cast<int64_t>( s ) );
}
constexpr QMTime operator"" _hz( unsigned long long int s ) {
	return QMTime::FromMilliseconds( static_cast<int64_t>( ( 1.0 / s ) * 1000 ) );
}

#define SERVER_TICK_RATE gi.tick_rate // in hz
extern QMTime FRAME_TIME_S;
extern QMTime FRAME_TIME_MS;

