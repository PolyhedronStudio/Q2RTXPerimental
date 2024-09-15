# Lua 5.4.7
project( lua-5.4.7 LANGUAGES C )

# Library Source Files:
SET( LUA_LIB_SOURCES 
	src/lapi.c 
	src/lcode.c 
	src/lctype.c 
	src/ldebug.c 
	src/ldo.c 
	src/ldump.c 
	src/lfunc.c 
	src/lgc.c 
	src/llex.c 
	src/lmem.c 
	src/lobject.c 
	src/lopcodes.c 
	src/lparser.c 
	src/lstate.c 
	src/lstring.c 
	src/ltable.c 
	src/ltm.c 
	src/lundump.c 
	src/lvm.c 
	src/lzio.c 
	src/lauxlib.c 
	src/lbaselib.c 
	src/lcorolib.c 
	src/ldblib.c 
	src/liolib.c 
	src/lmathlib.c 
	src/loadlib.c 
	src/loslib.c 
	src/lstrlib.c 
	src/ltablib.c 
	src/lutf8lib.c 
	src/linit.c 
)
# Interpreter Source Files:
SET( LUA_INTERPRETER_SOURCES src/lua.c )
# Compiler Source Files:
SET( LUA_COMPILER_SOURCES src/luac.c )


#
# liblua-5.4.7
#
if (WIN32)
	add_library( liblua-5.4.7 STATIC ${LUA_LIB_SOURCES} )
else()
	add_library( liblua-5.4.7 STATIC ${LUA_LIB_SOURCES} )
endif()
# Ensure the library ends up in the lua folder.
set_target_properties( liblua-5.4.7 
	PROPERTIES
	# Linux:
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
	LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/"
	LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/"
	LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/"
	LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/"
	# Windows:
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/"
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/"
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/"

	# No Prefixes.
	PREFIX ""
	DEBUG_POSTFIX "_d"
)


#
# Lua (Interpreter):
#
add_executable( lua ${LUA_INTERPRETER_SOURCES} )
target_link_libraries( lua liblua-5.4.7 )
# Ensure the Lua(Interpreter) ends up in the tools folder.
set_target_properties( lua
	PROPERTIES
	# Linux:
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/tools/"
	LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/tools/"
	LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/tools/"
	LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/tools/"
	LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/tools/"
	# Windows:
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/tools/"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/tools/"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/tools/"
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/tools/"
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/tools/"
	# No Prefixes.
	PREFIX ""
	DEBUG_POSTFIX "_d"
)

#
# LuaC (compiler):
#
add_executable( luac ${LUA_COMPILER_SOURCES} )
target_link_libraries( luac liblua-5.4.7 )
# Ensure the Lua(Compiler) ends up in the tools folder.
set_target_properties( luac
	PROPERTIES
	# Linux:
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/tools/"
	LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/tools/"
	LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/tools/"
	LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/tools/"
	LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/tools/"
	# Windows:
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/tools/"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/tools/"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/tools/"
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/tools/"
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/tools/"
	# No Prefixes.
	PREFIX ""
	DEBUG_POSTFIX "_d"
)