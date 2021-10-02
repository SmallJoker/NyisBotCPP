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
my_add_module("nbm_superior_uno")
my_add_module("nbm_timebomb")
