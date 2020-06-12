cmake_minimum_required(VERSION 3.0)
project(libstored)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
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

macro(libstored_generate target model)
    if(NOT EXISTS libstored)
        # Execute right away, such that the CMakeLists.txt exists.
        execute_process(
            COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/generator/generate.py -p ${target}- ${CMAKE_CURRENT_SOURCE_DIR}/${model} ${CMAKE_CURRENT_SOURCE_DIR}/libstored
        )
    endif()
    
    add_subdirectory(libstored)

    add_custom_command(
        OUTPUT ${target}-libstored.timestamp
        DEPENDS ${PROJECT_SOURCE_DIR}/include/libstored/store.h.tmpl
        DEPENDS ${PROJECT_SOURCE_DIR}/src/store.cpp.tmpl
        DEPENDS ${PROJECT_SOURCE_DIR}/CMakeLists.txt.tmpl
        DEPENDS ${model}
        COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/generator/generate.py -p ${target}- ${CMAKE_CURRENT_SOURCE_DIR}/${model} ${CMAKE_CURRENT_SOURCE_DIR}/libstored
        COMMAND ${CMAKE_COMMAND} -E touch ${target}-libstored.timestamp
        COMMENT "Generate store from ${model}"
        VERBATIM
    )
    add_custom_target(${target}-libstored-generate
        DEPENDS ${target}-libstored.timestamp
    )
    add_dependencies(${target}-libstored ${target}-libstored-generate)
    target_link_libraries(${target} PUBLIC ${target}-libstored)

    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
        target_link_options(${target} PUBLIC -Wl,--gc-sections)
    endif()
endmacro()

option(LIBSTORED_EXAMPLES "Build examples" ON)
if(LIBSTORED_EXAMPLES)
    add_subdirectory(examples)
endif()

