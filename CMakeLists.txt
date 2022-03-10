# Set up project
cmake_minimum_required(VERSION 3.6)
project(NyisBotCPP)
set(CMAKE_CXX_STANDARD 17)

message("--- Setting up NyisBotCPP")


### Libraries

find_package(CURL REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED) # pthread
find_package(pugixml REQUIRED)

add_definitions(-DPICOJSON_USE_INT64)
find_path(PICOJSON_INCLUDE_DIR picojson.h)

# More packages:
# gason: https://github.com/jyaif/gason


### Compiler options

if (NOT MSVC)
	# -O3 is default for release
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

	# Debugging symbols
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS} -g -O0")

	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endif()


### Source files

add_subdirectory("${PROJECT_SOURCE_DIR}/client")
add_subdirectory("${PROJECT_SOURCE_DIR}/core")
add_subdirectory("${PROJECT_SOURCE_DIR}/tests")

set(SRC_FILES
	${CLIENT_FILES}
	${CORE_FILES}
	${TESTS_FILES}
	main.cpp
)

include_directories(
	"${PROJECT_SOURCE_DIR}/core"
	${CURL_INCLUDE_DIRS}
	${PICOJSON_INCLUDE_DIR}
	pugixml
)


### Library creation

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE ${CURL_LIBRARIES}
	PRIVATE ${CMAKE_DL_LIBS}
	PRIVATE Threads::Threads
	PUBLIC pugixml
)


# ---------------- Loader ----------------

project(loader)

add_subdirectory("${PROJECT_SOURCE_DIR}/modules")

add_executable(${PROJECT_NAME} "loader.cpp")
target_link_libraries(
	${PROJECT_NAME}
	PRIVATE ${CMAKE_DL_LIBS}
)
