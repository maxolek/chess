cmake_minimum_required(VERSION 3.16)
project(tomahawk LANGUAGES CXX)

# -------------------
# Version
# -------------------
# Usage:
#   cmake -S . -B build -DVERSION=dev
#   cmake -S . -B build -DVERSION=1.2.3
if (NOT DEFINED VERSION)
    set(VERSION dev)
endif()

add_compile_definitions(ENGINE_ID="${VERSION}")
add_compile_definitions(PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

# -------------------
# C++ standard
# -------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------
# Directories
# -------------------
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(ENGINES_DIR ${CMAKE_SOURCE_DIR}/engines)

# -------------------
# Source files
# -------------------
set(SOURCES
    ${SRC_DIR}/board.cpp
    ${SRC_DIR}/gamestate.cpp
    ${SRC_DIR}/helpers.cpp
    ${SRC_DIR}/moveGenerator.cpp
    ${SRC_DIR}/magics.cpp
    ${SRC_DIR}/PrecomputedMoveData.cpp
    ${SRC_DIR}/UCI.cpp
    ${SRC_DIR}/book.cpp
    ${SRC_DIR}/searcher.cpp
    ${SRC_DIR}/NNUE.cpp
    ${SRC_DIR}/evaluator.cpp
    ${SRC_DIR}/engine.cpp
    ${SRC_DIR}/tomahawk.cpp
)

add_executable(tomahawk ${SOURCES})
target_include_directories(tomahawk PRIVATE ${INCLUDE_DIR})

# -------------------
# Dependency tracking for GCC/Clang
# -------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # -MMD : generate .d files for headers
    # -MP  : add phony targets
    # -MF  : optional output file, usually handled automatically by CMake
    target_compile_options(tomahawk PRIVATE -MMD -MP)
endif()


# -------------------
# Warnings & common flags
# -------------------
target_compile_options(tomahawk PRIVATE
    -Wall
    -Wextra
    -Wshadow
    -Wuninitialized
    -Wconversion
    -Wpedantic
)

# -------------------
# Build type logic
# -------------------
# Default to Release
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Flags by build type
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(tomahawk PRIVATE
        -O0
        -g
        -fno-optimize-sibling-calls
    )
    target_compile_definitions(tomahawk PRIVATE DEBUG)
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(tomahawk PRIVATE
        -O1
        -g
    )
else() # Release / MinSizeRel
    target_compile_options(tomahawk PRIVATE
        -O3
        -ffast-math
        -march=native
        -flto
    )
endif()

# -------------------
# Threads (portable)
# -------------------
find_package(Threads REQUIRED)
target_link_libraries(tomahawk PRIVATE Threads::Threads)

# -------------------
# Output directory
# -------------------
set_target_properties(tomahawk PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${ENGINES_DIR}
)

# Handle VERSION-based executable naming
if (VERSION AND NOT VERSION STREQUAL "dev")
    if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(tomahawk PROPERTIES
            OUTPUT_NAME ${VERSION}
        )
    endif()
endif()
