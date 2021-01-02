# Set up project
cmake_minimum_required(VERSION 3.6)
set(CMAKE_CXX_STANDARD 11)
project(NyisBotCPP)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED) # pthread

if (NOT MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

# Source files
set(SRC_FILES
	*.cpp
	modules/*.cpp
)

# Include Directories
include_directories(
	"${PROJECT_SOURCE_DIR}/inc"
	${CURL_INCLUDE_DIRS}
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(
	${PROJECT_NAME}
	${CURL_LIBRARIES}
	Threads::Threads
)
