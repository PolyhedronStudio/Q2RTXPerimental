#
# ---------- Main ----------------------------------------------
#
# Set minimum requirements.
cmake_minimum_required (VERSION 3.9)
cmake_policy(SET CMP0069 NEW)

####
##	Add our own 'module' path.
####
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

####
##	Main Project, and Version Number.
####
project(quake2-RTXPerimental)
set(Q2RTX_VERSION_MAJOR 0)
set(Q2RTX_VERSION_MINOR 0)
set(Q2RTX_VERSION_POINT 2)



#
# ---------- "Git Gut" -----------------------------------------
#
# get short-hash of repository.
execute_process(
	COMMAND git rev-parse --short HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE Q2RTX_VERSION_SHA
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

# get branch name of repository.
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE Q2RTX_VERSION_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)


#
# ---------- Build Options: ------------------------------------
#
OPTION(CONFIG_BUILD_CLIENT_TARGET "Build the Client target" ON)
OPTION(CONFIG_BUILD_SERVER_TARGET "Build the Dedicated Server target" ON)
OPTION(CONFIG_BUILD_GAME_BASEQ2_TARGETS "Build the BaseQ2 Game Module targets" ON)
OPTION(CONFIG_BUILD_GAME_BASEQ2RTXP_TARGETS "Build the BaseQ2RTXP Game Module targets" ON)
OPTION(CONFIG_GL_RENDERER "Enable GL renderer" ON)
OPTION(CONFIG_VKPT_RENDERER "Enable VKPT renderer" ON)
OPTION(CONFIG_VKPT_ENABLE_DEVICE_GROUPS "Enable device groups (multi-gpu) support" ON)
OPTION(CONFIG_VKPT_ENABLE_IMAGE_DUMPS "Enable image dumping functionality" OFF)
OPTION(CONFIG_USE_CURL "Use CURL for HTTP support" ON)
#OPTION(CONFIG_LINUX_PACKAGING_SUPPORT "Enable Linux Packaging support" OFF)
#OPTION(CONFIG_LINUX_PACKAGING_SKIP_PKZ "Skip zipping the game contents into .pkz when packaging (for quicker iteration)" OFF)
#OPTION(CONFIG_LINUX_STEAM_RUNTIME_SUPPORT "Enable Linux Steam Runtime support" OFF)
IF( CONFIG_BUILD_CLIENT_TARGET )
	IF(WIN32)
		SET(DEFAULT_BUILD_GLSLANG OFF)
	ELSE()
		SET(DEFAULT_BUILD_GLSLANG ON)
	ENDIF()
ENDIF() #IF( CONFIG_BUILD_CLIENT_TARGET )
OPTION(CONFIG_BUILD_GLSLANG "Build glslangValidator from source instead of using the SDK" ${DEFAULT_BUILD_GLSLANG})
OPTION(CONFIG_BUILD_IPO "Enable interprocedural optimizations" OFF)
OPTION(CONFIG_BUILD_SHADER_DEBUG_INFO "Build shaders with debug info" OFF)



#
# ---------- Set USE_FOLDERS globally --------------------------
#
set_property(GLOBAL PROPERTY USE_FOLDERS ON)



#
# ---------- Check for Inter Procedural Optimisations ----------
#
include(CheckIPOSupported)

IF(CONFIG_BUILD_IPO)
    check_ipo_supported(RESULT result OUTPUT output)
    if(result)
        MESSAGE(STATUS "Enabled interprocedural optimizations")
    else()
        MESSAGE(STATUS "Interprocedural optimizations not supported: ${output}")
        SET(CONFIG_BUILD_IPO OFF)
    endif()
ENDIF()



#
# ---------- Setup output Directories --------------------------
#
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY
   ${PROJECT_BINARY_DIR}/Bin
   CACHE PATH
   "Single Directory for all Libraries"
   )


#
# --------- Setup the Executable output Directory --------------
#
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Bin CACHE PATH "Single Directory for all Executables.")
   

#
# --------- 64 Bits Check --------------------------------------
#
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set( IS_64_BIT 1 )
endif ()


#
# --------- 'MSVC' Build Flag Management ------------
#
IF(MSVC)
    macro(replace_msvcrt var value)
        # Remove the existing /MD-type flags, if any
        string(REGEX REPLACE "/M[TD]d?\\s*" "" ${var} ${${var}})
        # Append the new flag
        set(${var} "${${var}} ${value}")
    endmacro(replace_msvcrt)
    
    replace_msvcrt(CMAKE_C_FLAGS_DEBUG "/MTd")
    replace_msvcrt(CMAKE_C_FLAGS_MINSIZEREL "/MT")
    replace_msvcrt(CMAKE_C_FLAGS_RELEASE "/MT")
    replace_msvcrt(CMAKE_C_FLAGS_RELWITHDEBINFO "/MT")

    # Enable multi-processor compilation when using MSBuild
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")

    # Also adjust C++ flags, needed for 3rd party code
    replace_msvcrt(CMAKE_CXX_FLAGS_DEBUG "/MTd")
    replace_msvcrt(CMAKE_CXX_FLAGS_MINSIZEREL "/MT")
    replace_msvcrt(CMAKE_CXX_FLAGS_RELEASE "/MT")
    replace_msvcrt(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MT")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
ELSEIF(CMAKE_SYSTEM_PROCESSOR STREQUAL "ppc64le")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=native")
ELSEIF(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2 -mfpmath=sse")
ENDIF()



#
# --------- Add 'External' libraries subdirectory --------------
#
# Work around CMAKE_INSTALL_PREFIX being a relative directory when using -B
# on Windows, which in turn trips up 'file(RELATIVE_PATH ...)' used in SDL2
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/extern/install)
add_subdirectory(extern)
link_directories(.)



#
# --------- Add 'Project' sources subdirectory -----------------
#

# First include the specific sources. (Put in a separate file to organize things a bit. )
include( ${CMAKE_CURRENT_SOURCE_DIR}/CMakeSources.cmake )
set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR}/install)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ${CONFIG_BUILD_IPO})
add_subdirectory(src)