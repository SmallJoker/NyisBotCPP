cmake_minimum_required(VERSION 3.9)
project(modules)
set(CMAKE_CXX_STANDARD 17)

message("--- Setting up Modules")

if(NOT MSVC)
	# For debugging
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")

	# Debugging symbols
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()

function(my_add_module)
	add_library(${ARGV0} SHARED "${ARGV0}.cpp")
	target_link_libraries(${ARGV0} PRIVATE NyisBotCPP)
endfunction()

my_add_module("nbm_builtin")
my_add_module("nbm_feeds")
my_add_module("nbm_liar_game")
my_add_module("nbm_quotes")
my_add_module("nbm_shithead")
my_add_module("nbm_superior_uno")
my_add_module("nbm_timebomb")

find_package(Lua)
if (LUA_FOUND)
	message("---> Found Lua ${LUA_VERSION_STRING}: Enabling module")

	project(lua_module)
	add_library(nbm_lua SHARED "nbm_lua.cpp")
	include_directories(${LUA_INCLUDE_DIR})
	target_link_libraries(nbm_lua PRIVATE NyisBotCPP ${LUA_LIBRARIES})
else()
	message("---> Lua not found. Module will be unavailable.")
endif()
