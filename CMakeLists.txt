cmake_minimum_required(VERSION 3.0)
project(libstored)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Debug FORCE)
endif()

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
  message(FATAL_ERROR "CMake generation is not allowed within the source directory! \
    Remove the CMakeCache.txt file and try again from another folder, e.g.: \
    \
      rm CMakeCache.txt \
      mkdir build \
      cd build \
      cmake ..")
endif()

set(PYTHON_EXECUTABLE "python3" CACHE STRING "Path to the python executable" FORCE)

macro(libstored_create target model)
    add_custom_command(
        OUTPUT ${target}-libstored.timestamp
        COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/generator/generate.py ${CMAKE_CURRENT_SOURCE_DIR}/${model} ${CMAKE_CURRENT_SOURCE_DIR}/libstored
        COMMAND ${CMAKE_COMMAND} -E touch ${target}-libstored.timestamp
        VERBATIM
    )
    add_custom_target(${target}-libstored
        DEPENDS ${target}-libstored.timestamp
    )
    add_dependencies(${target} ${target}-libstored)
    add_subdirectory(libstored)
endmacro()

option(LIBSTORED_EXAMPLES "Build examples" ON)
if(LIBSTORED_EXAMPLES)
    add_subdirectory(examples)
endif()

