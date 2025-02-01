/********************************************************************
*
*
*	SharedGame: QMTime, wraps up int64_t time management and adds additional support,
*				utility, operator and string literal accompanying features.
*				(100_ms, 100_sec etc.
*
*				Based on, and credits to Paril for the original implementation in Q2RE.
*
*
********************************************************************/
// Credits to Paril for the original in Q2RE.
#pragma once


#ifdef __cplusplus

/**
*	@brief	Stores a level time; most newer engines use int64_t for time storage,
*			but seconds are handy for compatibility with Quake and older mods.
**/
struct QMTime {
private:
	// times always start at zero, just to prevent memory issues
	int64_t _ms = 0;

	// internal; use _sec/_ms/_min or QMTime::FromMilliseconds(n)/QMTime::FromMilliseconds((n)/QMTime::from_min(n)
	constexpr explicit QMTime( const int64_t &ms ) : _ms( ms ) {
	}

public:
	constexpr QMTime() = default;
	constexpr QMTime( const QMTime & ) = default;
	constexpr QMTime &operator=( const QMTime & ) = default;

	// constructors are here, explicitly named, so you always
	// know what you're getting.

	/**
	*	@brief	New time from Milliseconds.
	**/
	static constexpr QMTime FromMilliseconds( const int64_t &ms ) {
		return QMTime( ms );
	}

	/**
	*	@brief	New time from Seconds.
	**/
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	static constexpr QMTime FromSeconds( const T &s ) {
		return QMTime( static_cast<int64_t>( s * 1000 ) );
	}
	/**
	*	@brief	New time from Minutes
	**/
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	static constexpr QMTime FromMinutes( const T &s ) {
		return QMTime( static_cast<int64_t>( s * 60000 ) );
	}
	/**
	*	@brief	New time from Hz
	**/
	static constexpr QMTime FromHz( const uint64_t &hz ) {
		return FromMilliseconds( static_cast<int64_t>( ( 1.0 / hz ) * 1000 ) );
	}

	/**
	*	@return	The time value in Minutes. (truncated for integers).
	**/
	template<typename T = double>
	constexpr inline T Minutes() const {
		return static_cast<T>( _ms / static_cast<std::conditional_t<std::is_floating_point_v<T>, T, float>>( 60000 ) );
	}

	/**
	*	@return	The time value in Seconds. (truncated for integers).
	**/
	template<typename T = double>
	constexpr inline T Seconds() const {
		return static_cast<T>( _ms / static_cast<std::conditional_t<std::is_floating_point_v<T>, T, float>>( 1000 ) );
	}

	/**
	*	@return	The time value in Milliseconds.
	**/
	constexpr const inline int64_t &Milliseconds() const {
		return _ms;
	}

	/**
	*	@return	The number of frames have passed for the current time value.
	**/
	inline int64_t Frames() const;;

	/**
	*	@brief	Check if 'non-zero'.
	**/
	constexpr explicit operator bool() const {
		return !!_ms;
	}

	/**
	*	@brief	Invert Time.
	**/
	constexpr inline QMTime operator-() const {
		return QMTime( -_ms );
	}

	/**
	*	@brief	Operations with other times as input.
	**/
	constexpr inline QMTime operator+( const QMTime &r ) const {
		return QMTime( _ms + r._ms );
	}
	constexpr inline QMTime operator-( const QMTime &r ) const {
		return QMTime( _ms - r._ms );
	}
	constexpr inline QMTime &operator+=( const QMTime &r ) {
		return *this = *this + r;
	}
	constexpr inline QMTime &operator-=( const QMTime &r ) {
		return *this = *this - r;
	}

	/**
	*	@brief	Operations with scalars as input.
	**/
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr inline QMTime operator*( const T &r ) const {
		return QMTime::FromMilliseconds( ( static_cast<int64_t>( _ms * r ) ) );
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr inline QMTime operator/( const T &r ) const {
		return QMTime::FromMilliseconds( ( static_cast<int64_t>( _ms / r ) ) );
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr inline QMTime &operator*=( const T &r ) {
		return *this = *this * r;
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr inline QMTime &operator/=( const T &r ) {
		return *this = *this / r;
	}

	/**
	*	@brief	Comparison with same time.
	**/
	constexpr inline bool operator==( const QMTime &time ) const {
		return _ms == time._ms;
	}
	constexpr inline bool operator!=( const QMTime &time ) const {
		return _ms != time._ms;
	}
	constexpr inline bool operator<( const QMTime &time ) const {
		return _ms < time._ms;
	}
	constexpr inline bool operator>( const QMTime &time ) const {
		return _ms > time._ms;
	}
	constexpr inline bool operator<=( const QMTime &time ) const {
		return _ms <= time._ms;
	}
	constexpr inline bool operator>=( const QMTime &time ) const {
		return _ms >= time._ms;
	}
	/**
	*	@brief	Comparison with same time.
	**/
	template<typename T>
	constexpr inline bool operator==( const T &time ) const {
		return _ms == time._ms;
	}
	template<typename T>
	constexpr inline bool operator!=( const T &time ) const {
		return _ms != time._ms;
	}
	template<typename T>
	constexpr inline bool operator<( const T &time ) const {
		return _ms < time._ms;
	}
	template<typename T>
	constexpr inline bool operator>( const T &time ) const {
		return _ms > time._ms;
	}
	template<typename T>
	constexpr inline bool operator<=( const T &time ) const {
		return _ms <= time._ms;
	}
	template<typename T>
	constexpr inline bool operator>=( const T &time ) const {
		return _ms >= time._ms;
	}
};

// user literals, allowing you to specify times
// as 128_sec and 128_ms
constexpr inline QMTime operator"" _min( long double s ) {
	return QMTime::FromMinutes( s );
}
constexpr inline QMTime operator"" _min( unsigned long long int s ) {
	return QMTime::FromMinutes( s );
}
constexpr inline QMTime operator"" _sec( long double s ) {
	return QMTime::FromSeconds( s );
}
constexpr inline QMTime operator"" _sec( unsigned long long int s ) {
	return QMTime::FromSeconds( s );
}
constexpr inline QMTime operator"" _ms( long double s ) {
	return QMTime::FromMilliseconds( static_cast<int64_t>( s ) );
}
constexpr inline QMTime operator"" _ms( unsigned long long int s ) {
	return QMTime::FromMilliseconds( static_cast<int64_t>( s ) );
}
constexpr inline QMTime operator"" _hz( unsigned long long int s ) {
	return QMTime::FromMilliseconds( static_cast<int64_t>( ( 1.0 / s ) * 1000 ) );
}


/**
*
*
*	Time Extensions for C++ random mt19937.
*
*
**/
// uniform time [min_inclusive, max_exclusive)
[[nodiscard]] inline QMTime random_time( QMTime min_inclusive, QMTime max_exclusive ) {
	return QMTime::FromMilliseconds( std::uniform_int_distribution<int64_t>( min_inclusive.Milliseconds(), max_exclusive.Milliseconds() )( mt_rand ) );
}

// uniform time [0, max_exclusive)
[[nodiscard]] inline QMTime random_time( QMTime max_exclusive ) {
	return QMTime::FromMilliseconds( std::uniform_int_distribution<int64_t>( 0, max_exclusive.Milliseconds() )( mt_rand ) );
}

#endif // __cplusplus