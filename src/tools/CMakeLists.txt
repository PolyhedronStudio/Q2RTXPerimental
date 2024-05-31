set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

#set_property( TARGET client PROPERTY CXX_STANDARD 20 )
#set_property( TARGET client PROPERTY CXX_STANDARD_REQUIRED 20 )
#set_property( TARGET client PROPERTY CXX_EXTENSIONS OFF )
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

####
##	Embed GIT Repository Version Number as compiler parameter defines.
####
add_definitions(-DVERSION_MAJOR=${Q2RTX_VERSION_MAJOR})
add_definitions(-DVERSION_MINOR=${Q2RTX_VERSION_MINOR})
add_definitions(-DVERSION_POINT=${Q2RTX_VERSION_POINT})
add_definitions(-DVERSION_SHA=${Q2RTX_VERSION_SHA})
add_definitions(-DVERSION_BRANCH=${Q2RTX_VERSION_BRANCH})

add_definitions(-DHAVE_CONFIG_H=1 -DCURL_STATICLIB -DAL_LIBTYPE_STATIC)

IF( WIN32 )
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
ENDIF()
add_compile_definitions($<$<CONFIG:Debug>:USE_DEBUG>)
if( NOT WIN32 )
    add_compile_definitions(_GNU_SOURCE)
endif()

# Check whether compiler supports 'flagstr', and if so, set ${flagvar} to that value
function(CCompilerFlagString flagvar flagstr)
    CHECK_C_COMPILER_FLAG("${flagstr}" ${flagvar}_FOUND)
    if(${flagvar}_FOUND)
        set(${flagvar} "${flagstr}" PARENT_SCOPE)
    endif()
endfunction()
# Check whether compiler supports 'flagstr', and if so, append that value to ${flagvar}
function(CCompilerFlagStringAppend flagvar flagstr)
    string(TOUPPER "${flagstr}" auto_flag_var)
    string(REGEX REPLACE "[^A-Z0-9]" "_" auto_flag_var "${auto_flag_var}")
    CCompilerFlagString (C_${auto_flag_var} "${flagstr}")
    if(NOT "${C_${auto_flag_var}}" EQUAL "")
        set(${flagvar} "${${flagvar}} ${C_${auto_flag_var}} " PARENT_SCOPE)
    endif()
endfunction()

if( NOT MSVC )
    CCompilerFlagStringAppend(CMAKE_C_FLAGS "-fsigned-char")
    CCompilerFlagStringAppend(CMAKE_C_FLAGS "-Wall")
    CCompilerFlagStringAppend(CMAKE_C_FLAGS "-Wpointer-arith")
    CCompilerFlagStringAppend(CMAKE_C_FLAGS "-Wformat-security")
    CCompilerFlagStringAppend(CMAKE_C_FLAGS "-Werror=vla")
    CCompilerFlagStringAppend(CMAKE_C_FLAGS "-Wstrict-prototypes")
    # CCompilerFlagString(WARN_MISSING_PROTOTYPES "-Wmissing-prototypes")
endif()
