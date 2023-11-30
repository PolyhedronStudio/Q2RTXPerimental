set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

#set_property( TARGET client PROPERTY CXX_STANDARD 20 )
#set_property( TARGET client PROPERTY CXX_STANDARD_REQUIRED 20 )
#set_property( TARGET client PROPERTY CXX_EXTENSIONS OFF )
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )


####
##	Build VKPT shaders ONLY if the client is being built, with VKPT renderer enabled.
####
IF( CONFIG_BUILD_CLIENT_TARGET AND CONFIG_VKPT_RENDERER )
    include(../cmake/compileShaders.cmake)

    foreach(s ${SRC_RT_SHADERS})
        get_filename_component(shader_name ${s} NAME)
        get_filename_component(shader_ext ${s} EXT)
        compile_shader(SOURCE_FILE ${s} OUTPUT_FILE_LIST shader_bytecode OUTPUT_FILE_NAME "${shader_name}.pipeline")
        if (shader_ext STREQUAL ".rgen")
            compile_shader(SOURCE_FILE ${s} OUTPUT_FILE_LIST shader_bytecode OUTPUT_FILE_NAME "${shader_name}.query" DEFINES "-DKHR_RAY_QUERY" STAGE "comp")
        endif()
    endforeach()

    foreach(s ${SRC_SHADERS})
        compile_shader(SOURCE_FILE ${s} INCLUDES "-I${CMAKE_CURRENT_SOURCE_DIR}/refresh/vkpt/fsr" OUTPUT_FILE_LIST shader_bytecode)
    endforeach()

    add_custom_target(shaders DEPENDS ${shader_bytecode})

    if(TARGET glslangValidator)
        add_dependencies(shaders glslangValidator)
    endif()
ENDIF() #IF( CONFIG_BUILD_CLIENT_TARGET AND CONFIG_VKPT_RENDERER )

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



####
##	Build Target: "Client":
####
IF( CONFIG_BUILD_CLIENT_TARGET )
	# Add Build Target:
	IF( WIN32 )
		add_executable(client WIN32 
			${SRC_CLIENT} ${HEADERS_CLIENT} 
			${SRC_COMMON} ${HEADERS_COMMON} 
			${SRC_REFRESH} ${SRC_SHADERS} 
			${SRC_SHARED} 
			${SRC_WINDOWS} ${SRC_WINDOWS_CLIENT} ${HEADERS_WINDOWS}
			${SRC_SERVER} ${HEADERS_SERVER}
			windows/res/q2rtxp.rc
		)
	ELSE()
		add_executable(client
			${SRC_CLIENT} ${HEADERS_CLIENT} 
			${SRC_COMMON} ${HEADERS_COMMON} 
			${SRC_REFRESH} ${SRC_SHADERS} 
			${SRC_SHARED} 
			${SRC_LINUX} ${SRC_LINUX_CLIENT}
			${SRC_SERVER} ${HEADERS_SERVER}
		)
	ENDIF()

	# Compile Definitions:
    target_compile_definitions(client PRIVATE USE_CLIENT=1 USE_FIXED_LIBAL=1 USE_SDL=1)
	# Compile Options:
    target_compile_options(client PRIVATE "${WARN_MISSING_PROTOTYPES}")

	# LibCURL:
    IF( CONFIG_USE_CURL )
        target_sources(client PRIVATE ${SRC_CLIENT_HTTP})
        target_compile_definitions(client PRIVATE USE_CURL=1)
        target_link_libraries(client libcurl)
    ENDIF() #IF( CONFIG_USE_CURL )

	# IF Enabled: add "OpenGL Renderer" sources to the client's source list.
    IF( CONFIG_GL_RENDERER )
		# Add OpenGL Renderer sources.
		target_sources(client PRIVATE ${SRC_GL} ${HEADERS_GL})
		# Define for in-source use, REF_GL=1 and USE_REF=1.
		target_compile_definitions(client PRIVATE REF_GL=1 USE_REF=1)
    ENDIF() #IF( CONFIG_GL_RENDERER )

	# IF Enabled: add "Vulken Renderer" sources to the client's source list.
    IF( CONFIG_VKPT_RENDERER )
		# Add "Shaders" fake target as a dependency.
		add_dependencies(client shaders)

		# Add VKPT Renderer sources.
        target_sources(client PRIVATE ${SRC_VKPT} ${HEADERS_VKPT})
		# Add in Vulkan header includes.
        target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/extern/Vulkan-Headers/include)
        # Set link directories.
		link_directories(client PRIVATE ${CMAKE_SOURCE_DIR})
		# Define for in-source use, REF_VKPT=1 and USE_REF=1.
        target_compile_definitions(client PRIVATE REF_VKPT=1 USE_REF=1)
		# Refresh Subsystem Setting Compile Definitions:
        IF(CONFIG_VKPT_ENABLE_DEVICE_GROUPS)
            target_compile_definitions(client PRIVATE VKPT_DEVICE_GROUPS)
        ENDIF()
        IF(CONFIG_VKPT_ENABLE_IMAGE_DUMPS)
            target_compile_definitions(client PRIVATE VKPT_IMAGE_DUMPS)
        ENDIF()

		# Specific Vulkan lib to link to depending on OS.
        IF( WIN32 )
			target_link_libraries(client vulkan-1)
        ELSE ()
	        target_link_libraries(client vulkan)
        ENDIF() #IF( WIN32 )
    ENDIF()#IF( CONFIG_VKPT_RENDERER )

    target_link_libraries(client stb)
    target_link_libraries(client tinyobjloader)
	target_link_libraries(client OpenAL)
	target_link_libraries(client SDL2main SDL2-static zlibstatic)

	# Other WIN32 related options:
	IF( WIN32 ) 
        target_include_directories(client PRIVATE ../VC/inc)
        target_link_libraries(client winmm ws2_32)
        set_target_properties(client PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        # macro redefinition, deprecation
        target_compile_options(client PRIVATE /wd4996) 
		#TARGET_COMPILE_OPTIONS(client PRIVATE /wd4005 /wd4996)
	ENDIF()

	target_include_directories(client PRIVATE ../inc)
    target_include_directories(client PRIVATE "${ZLIB_INCLUDE_DIRS}")

	# Set the actual final binary output properties of the client.
	set_target_properties(client
        PROPERTIES
        OUTPUT_NAME "Q2RTXPerimental"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}"
        DEBUG_POSTFIX ""
    )
ENDIF() #IF( CONFIG_BUILD_CLIENT_TARGET )



####
##	Build Target: "Dedicated Server":
####
IF( CONFIG_BUILD_SERVER_TARGET )
	# Add Build Target:
	IF( WIN32 )
		ADD_EXECUTABLE(server 
			${SRC_COMMON} ${HEADERS_COMMON} 
			${SRC_SHARED} 
			${SRC_WINDOWS} ${HEADERS_WINDOWS}
			${SRC_SERVER} ${HEADERS_SERVER}
			client/null.cpp
			windows/res/q2rtxpded.rc
		)
	ELSE()
		ADD_EXECUTABLE(server
			${SRC_COMMON} ${HEADERS_COMMON} 
			${SRC_SHARED} 
			${SRC_LINUX}
			${SRC_SERVER} ${HEADERS_SERVER}
			client/null.cpp
		)
	ENDIF()#IF(WIN32)

	target_compile_definitions(server PRIVATE USE_SERVER=1)
	target_compile_options(server PRIVATE "${WARN_MISSING_PROTOTYPES}")

	IF( WIN32 )
		target_include_directories(server PRIVATE ../VC/inc)
		target_link_libraries(server winmm ws2_32)
		set_target_properties(server PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

		# macro redefinition, deprecation
		target_compile_options(server PRIVATE /wd4996)
		#TARGET_COMPILE_OPTIONS(server PRIVATE /wd4005 /wd4996)
	ENDIF()#IF(WIN32)

	# General include directory.
	target_include_directories(server PRIVATE ../inc)

	# ZLib:
	target_include_directories(server PRIVATE "${ZLIB_INCLUDE_DIRS}")
	target_link_libraries(server zlibstatic)

	# Unix Specific Link Libraries:
	IF( UNIX )
		target_link_libraries(server dl rt m pthread)
	ENDIF() # IF( UNIX )

	set_target_properties(server
		PROPERTIES
		OUTPUT_NAME "Q2RTXPerimental_ded"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}"
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}"
		DEBUG_POSTFIX ""
	)
ENDIF()#IF( CONFIG_BUILD_SERVER_TARGET )



####
##	Build Targets: "BaseQ2 Game":
####
IF( CONFIG_BUILD_GAME_BASEQ2_TARGETS )
	# Server Game:
	add_library(baseq2_svgame SHARED 
		${SRC_BASEQ2_SVGAME} ${HEADERS_BASEQ2_SVGAME} 
		${SRC_BASEQ2_SHAREDGAME} ${HEADERS_BASEQ2_SHAREDGAME} 
		${SRC_SHARED}
	)
	# Client Game:
	add_library(baseq2_clgame SHARED 
		${SRC_BASEQ2_CLGAME} ${HEADERS_BASEQ2_CLGAME} 
		${SRC_BASEQ2_SHAREDGAME} ${HEADERS_BASEQ2_SHAREDGAME} 
		${SRC_SHARED}
	)

	IF( CMAKE_C_COMPILER_ID STREQUAL "GNU" )
		# Workaround for GCC 10 linking shared libgcc by default
		target_link_options(baseq2_clgame PRIVATE "-static-libgcc")
		target_link_options(baseq2_svgame PRIVATE "-static-libgcc")
	ENDIF() #IF( CMAKE_C_COMPILER_ID STREQUAL "GNU" )

	# WIN32 Specifics:
	IF( WIN32 )
		target_include_directories(baseq2_svgame PRIVATE ../VC/inc)
		target_include_directories(baseq2_clgame PRIVATE ../VC/inc)
		target_sources(baseq2_svgame PRIVATE windows/res/baseq2_svgame.rc)
		target_sources(baseq2_clgame PRIVATE windows/res/baseq2_clgame.rc)
		# macro redefinition, deprecation
		target_compile_options(baseq2_svgame PRIVATE /wd4996)
		#target_compile_options(baseq2_svgame PRIVATE /wd4005 /wd4996)
		target_compile_options(baseq2_clgame PRIVATE /wd4996)
		#target_compile_options(baseq2_svgame PRIVATE /wd4005 /wd4996)
	ENDIF() #IF( WIN32 ) 

	target_include_directories(baseq2_svgame PRIVATE ../inc)
	target_include_directories(baseq2_clgame PRIVATE ../inc)
    target_link_libraries(baseq2_svgame zlibstatic)

	# Specify both LIBRARY and RUNTIME because one works only on Windows,
	# and another works only on Linux.
	# "Server Game" Target Properties:
	set_target_properties(baseq2_svgame
		PROPERTIES
		# Linux:
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/baseq2"
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/baseq2"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/baseq2"
		LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/baseq2"
		LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/baseq2"
		# Windows:
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/baseq2"
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/baseq2"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/baseq2"
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/baseq2"
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/baseq2"
		# No Prefixes.
		PREFIX ""
		DEBUG_POSTFIX ""
	)
	# "Client Game" Target Properties:
	set_target_properties(baseq2_clgame
		PROPERTIES
		# Linux:
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/baseq2"
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/baseq2"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/baseq2"
		LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/baseq2"
		LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/baseq2"
		# Windows:
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/baseq2"
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/baseq2"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/baseq2"
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/baseq2"
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/baseq2"
		# No Prefixes.
		PREFIX ""
		DEBUG_POSTFIX ""
	)

	# Base binary output names on actual build architecture.
	IF( CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" )
		SET_TARGET_PROPERTIES(baseq2_svgame
			PROPERTIES
			LIBRARY_OUTPUT_NAME "svgameaarch64"
			RUNTIME_OUTPUT_NAME "svgameaarch64"
		)
		SET_TARGET_PROPERTIES(baseq2_clgame
			PROPERTIES
			LIBRARY_OUTPUT_NAME "clgameaarch64"
			RUNTIME_OUTPUT_NAME "clgameaarch64"
		)
	ELSEIF( CMAKE_SYSTEM_PROCESSOR STREQUAL "ppc64le" )
		SET_TARGET_PROPERTIES(baseq2_svgame
			PROPERTIES
			LIBRARY_OUTPUT_NAME "svgameppc64le"
			RUNTIME_OUTPUT_NAME "svgameppc64le"
		)
		SET_TARGET_PROPERTIES(baseq2_clgame
			PROPERTIES
			LIBRARY_OUTPUT_NAME "clgameppc64le"
			RUNTIME_OUTPUT_NAME "clgameppc64le"
		)
	ELSE()
		SET_TARGET_PROPERTIES(baseq2_svgame
			PROPERTIES
			LIBRARY_OUTPUT_NAME "svgamex86_64"
			RUNTIME_OUTPUT_NAME "svgamex86_64"
		)
		SET_TARGET_PROPERTIES(baseq2_clgame
			PROPERTIES
			LIBRARY_OUTPUT_NAME "clgamex86_64"
			RUNTIME_OUTPUT_NAME "clgamex86_64"
		)
	ENDIF()
ENDIF() #IF( CONFIG_BUILD_GAME_BASEQ2_TARGETS )

####
##	MSVC Source Groups support for CMake generated "Solutions":
####
IF ( MSVC )
	SOURCE_GROUP("baseq2\\svgame\\sources" FILES ${SRC_BASEQ2_SVGAME})
	SOURCE_GROUP("baseq2\\svgame\\headers" FILES ${HEADERS_BASEQ2_SVGAME})
	SOURCE_GROUP("baseq2\\clgame\\sources" FILES ${SRC_BASEQ2_CLGAME})
	SOURCE_GROUP("baseq2\\clgame\\headers" FILES ${HEADERS_BASEQ2_CLGAME})
	SOURCE_GROUP("client\\sources" FILES ${SRC_CLIENT})
	SOURCE_GROUP("client\\headers" FILES ${HEADERS_CLIENT})
	SOURCE_GROUP("server\\sources" FILES ${SRC_SERVER})
	SOURCE_GROUP("server\\headers" FILES ${HEADERS_SERVER})
	SOURCE_GROUP("common\\sources" FILES ${SRC_COMMON})
	SOURCE_GROUP("common\\headers" FILES ${HEADERS_COMMON})
	SOURCE_GROUP("refresh\\sources" FILES ${SRC_REFRESH} ${SRC_VKPT})
	SOURCE_GROUP("refresh\\headers" FILES ${HEADERS_VKPT})
	SOURCE_GROUP("refresh\\shaders" FILES ${SRC_SHADERS})
	SOURCE_GROUP("shared\\sources" FILES ${SRC_SHARED})
	SOURCE_GROUP("windows\\sources" FILES ${SRC_WINDOWS})
	SOURCE_GROUP("windows\\headers" FILES ${HEADERS_WINDOWS})
ENDIF() #IF ( MSVC )