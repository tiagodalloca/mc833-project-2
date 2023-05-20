cmake_minimum_required(VERSION 3.5)

project(server)

# Add the C standard to use
set(CMAKE_C_STANDARD 11)

# Add your source files
set(SERVER_SOURCE_FILES src/server.c)

# Find the JSON-C library
find_package(PkgConfig REQUIRED)
pkg_search_module(JSONC REQUIRED json-c)
include_directories(${JSONC_INCLUDE_DIRS})

# Create the executable
add_executable(server ${SERVER_SOURCE_FILES})

# Link against the JSON-C library
target_link_libraries(server ${JSONC_LIBRARIES})
