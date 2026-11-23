cmake_minimum_required(VERSION 3.25)

project(strictor LANGUAGES CXX)

set(LLVM_PATH "$ENV{ProgramW6432}/LLVM" CACHE PATH "LLVM installation that provides libclang and clang-format")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Output")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

add_subdirectory(StrictorCore)
add_subdirectory(strictor)
add_subdirectory(StrictorTests)
