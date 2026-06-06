cmake_minimum_required(VERSION 3.16)
project(minigit VERSION 1.0.0 LANGUAGES CXX)

# ── Compiler settings ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# Strict warnings — makes the code interview-ready
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

# ── Build type default ─────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ── Source files ───────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/HashUtility.cpp
    src/FileUtils.cpp
    src/Commit.cpp
    src/ObjectStore.cpp
    src/StagingManager.cpp
    src/CommitManager.cpp
    src/FileTracker.cpp
    src/RepositoryManager.cpp
    src/DiffEngine.cpp
    src/CommandParser.cpp
)

# ── Executable ─────────────────────────────────────────────────────────────────
add_executable(minigit ${SOURCES})

target_include_directories(minigit PRIVATE ${CMAKE_SOURCE_DIR}/include)

# ── Install (optional) ─────────────────────────────────────────────────────────
install(TARGETS minigit DESTINATION bin)

# ── Build summary ─────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "  minigit ${PROJECT_VERSION}")
message(STATUS "  Build type  : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler    : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
