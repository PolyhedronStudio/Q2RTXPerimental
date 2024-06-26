####
##	ZLIB
####
#if (NOT CONFIG_LINUX_STEAM_RUNTIME_SUPPORT)
add_subdirectory(zlib)
target_include_directories(zlibstatic PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zlib> $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/zlib>)

set_target_properties(zlib PROPERTIES FOLDER extern)
set_target_properties(zlibstatic PROPERTIES FOLDER extern POSITION_INDEPENDENT_CODE ON)
set_target_properties(minigzip PROPERTIES FOLDER extern)
set_target_properties(example PROPERTIES FOLDER extern)
#endif()

####
##	glslangValidator
####
if( CONFIG_BUILD_CLIENT_TARGET AND CONFIG_VKPT_RENDERER AND CONFIG_BUILD_GLSLANG)
    option(BUILD_EXTERNAL OFF)
    option(ENABLE_SPVREMAPPER OFF)
    option(ENABLE_HLSL OFF)
    option(ENABLE_OPT OFF)
    option(ENABLE_PCH OFF)
    option(ENABLE_CTEST OFF)

    add_subdirectory(glslang)

    set_target_properties(glslangValidator PROPERTIES FOLDER glslang)
    set_target_properties(glslang-build-info PROPERTIES FOLDER glslang)
endif()

####
##	SDL 2
####
# On a 32-bit target, only the server is built, and it doesn't need SDL
if( CONFIG_BUILD_CLIENT_TARGET )
    option(SDL_SHARED "" OFF)
    option(SDL_STATIC "" ON)
    option(SDL_SHARED_ENABLED_BY_DEFAULT OFF)
    option(SDL_TEST OFF)

    #IF(CONFIG_LINUX_STEAM_RUNTIME_SUPPORT)
    #	option(INPUT_TSLIB OFF)
    #ENDIF()

    add_subdirectory(SDL2)

    set_target_properties(SDL2main PROPERTIES FOLDER extern)
    set_target_properties(SDL2-static PROPERTIES FOLDER extern)
    set_target_properties(uninstall PROPERTIES FOLDER extern)
    set_target_properties(sdl_headers_copy PROPERTIES FOLDER extern)
ENDIF() #IF ( CONFIG_BUILD_CLIENT_TARGET )

#
# CURL
#
IF( CONFIG_USE_CURL )
    option(BUILD_CURL_EXE "" OFF)
    option(BUILD_SHARED_LIBS "" OFF)
    option(CURL_STATICLIB "" ON)
    option(BUILD_TESTING "" OFF)
    option(ENABLE_MANUAL "" OFF)
    option(ENABLE_DOCS "" OFF)
    option(HTTP_ONLY "" ON)
    option(CURL_ZLIB "" OFF)
    option(CMAKE_USE_OPENSSL "" OFF)
    set(CURL_CA_PATH "none" CACHE PATH "")

    set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "Override CMAKE_DEBUG_POSTFIX, which curl sets to '-d'")
    
    set(ZLIB_DIR "{CMAKE_CURRENT_SOURCE_DIR}/zlib")

    add_subdirectory(curl)

    set_target_properties(libcurl_static PROPERTIES FOLDER extern)
    if (TARGET libcurl_object)
        set_target_properties(libcurl_object PROPERTIES FOLDER extern)
    endif()
    set_target_properties(curl_uninstall PROPERTIES FOLDER extern)
endif()


####
##	openal-soft (not needed for the server)
####
IF ( CONFIG_BUILD_CLIENT_TARGET )
	add_library(stb INTERFACE)
	target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)

	add_library(tinyobjloader INTERFACE)
	target_include_directories(tinyobjloader INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader-c)


    set(LIBTYPE STATIC)
    option(FORCE_STATIC_VCRT "" ON)
    option(ALSOFT_UTILS "" OFF)
    option(ALSOFT_EXAMPLES "" OFF)
    option(ALSOFT_UPDATE_BUILD_VERSION "" OFF)
    option(ALSOFT_INSTALL "" OFF)
    option(ALSOFT_INSTALL_CONFIG "" OFF)
    option(ALSOFT_INSTALL_HRTF_DATA "" OFF)
    option(ALSOFT_INSTALL_AMBDEC_PRESETS "" OFF)
    option(ALSOFT_INSTALL_EXAMPLES "" OFF)
    option(ALSOFT_INSTALL_UTILS "" OFF)
    add_subdirectory(openal-soft)
    set_target_properties(OpenAL PROPERTIES FOLDER extern/openal-soft)
    set_target_properties(common PROPERTIES FOLDER extern/openal-soft)
    set_target_properties(ex-common PROPERTIES FOLDER extern/openal-soft)
ENDIF() # IF ( CONFIG_BUILD_CLIENT_TARGET )