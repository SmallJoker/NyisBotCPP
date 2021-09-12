cmake_minimum_required(VERSION 3.9)
project(modules)
set(CMAKE_CXX_STANDARD 17)

message("--- Setting up Modules")

if(NOT MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
	# For debugging
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")

	# Debugging symbols
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()

add_library(nbm_builtin SHARED
	builtin.cpp
)
target_link_libraries(nbm_builtin
	PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../libNyisBotCPP.so"
)

add_library(nbm_timebomb SHARED
	timebomb.cpp
)
#target_link_libraries(nbm_timebomb NyisBotCPP)
