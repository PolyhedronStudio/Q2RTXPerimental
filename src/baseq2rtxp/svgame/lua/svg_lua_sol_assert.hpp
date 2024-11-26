/********************************************************************
*
*
*	ServerGame: Lua SOL Asserts.
*
*
********************************************************************/
#ifndef SVGAME_LUA_SOL_ASSERT_HPP
#define SVGAME_LUA_SOL_ASSERT_HPP

#define STRINGIFY2(x)   #x
#define STRINGIFY(x)    STRINGIFY2(x)

// The Example Implementation:
#if 0
#define m_assert(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << " line " << __LINE__ << ": " << message << std::endl; \
            std::terminate(); \
        } \
    } while (false)

#define c_assert(condition) \
    do { \
        if (! (condition)) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << " line " << __LINE__ << std::endl; \
            std::terminate(); \
        } \
    } while (false)
// Q2RTXP Implementation:
#else 
#define m_assert(condition, message) \
    do { \
        if (! (condition)) { \
            gi.dprintf( "Assertion '"#condition"' failed in "STRINGIFY(__FILE__)":"STRINGIFY2(__LINE__)": "message"\n"); \
        } \
    } while (false)

#define c_assert(condition) \
    do { \
        if (! (condition)) { \
            gi.dprintf( "Assertion '"#condition"' failed in "STRINGIFY(__FILE__)":"STRINGIFY2(__LINE__)"\n"); \
        } \
    } while (false)    
#endif  // #if0
#else
    #define m_assert(condition, message) do { if (false) { (void)(condition); (void)sizeof(message); } } while (false)
    #define c_assert(condition) do { if (false) { (void)(condition); } } while (false)
#endif // EXAMPLES_ASSERT_HPP