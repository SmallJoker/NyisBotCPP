# Set up project
cmake_minimum_required(VERSION 3.6)
project(NyisBotCPP)
set(CMAKE_CXX_STANDARD 17)

message("--- Setting up NyisBotCPP")

find_package(CURL REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED) # pthread

if (NOT MSVC)
	# -O3 is default
	#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")

	# Debugging symbols
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()

add_subdirectory("${PROJECT_SOURCE_DIR}/core")
add_subdirectory("${PROJECT_SOURCE_DIR}/tests")

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

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})
#add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE ${CURL_LIBRARIES}
	PRIVATE ${CMAKE_DL_LIBS}
	PRIVATE Threads::Threads
)


# ---------------- Loader ----------------

project(loader)

add_subdirectory("${PROJECT_SOURCE_DIR}/modules")

add_executable(${PROJECT_NAME} "loader.cpp")
target_link_libraries(
	${PROJECT_NAME}
	PRIVATE ${CMAKE_DL_LIBS}
)
