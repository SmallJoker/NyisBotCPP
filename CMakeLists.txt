# Set up project
cmake_minimum_required(VERSION 3.6)
project(NyisBotCPP)
set(CMAKE_CXX_STANDARD 17)

message("--- Setting up NyisBotCPP")

find_package(CURL REQUIRED)
find_package(Threads REQUIRED) # pthread

if (NOT MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

	# Debugging symbols
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()

add_subdirectory("${PROJECT_SOURCE_DIR}/core")
add_subdirectory("${PROJECT_SOURCE_DIR}/tests")
add_subdirectory("${PROJECT_SOURCE_DIR}/modules")

# Source files
set(SRC_FILES
	${CORE_FILES}
	${TESTS_FILES}
	main.cpp
)

# Include Directories
include_directories(
	${PROJECT_SOURCE_DIR}
	${CURL_INCLUDE_DIRS}
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(
	${PROJECT_NAME}
	${CURL_LIBRARIES}
	${CMAKE_DL_LIBS}
	Threads::Threads
)
