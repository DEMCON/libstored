cmake_minimum_required(VERSION 3.0)
project(libstored)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
  message(FATAL_ERROR "CMake generation is not allowed within the source directory! \
    Remove the CMakeCache.txt file and try again from another folder, e.g.: \
    \
      rm CMakeCache.txt \
      mkdir build \
      cd build \
      cmake ..")
endif()

find_package(Doxygen)
option(LIBSTORED_DOCUMENTATION "Create the HTML based API documentation (requires Doxygen)" ${DOXYGEN_FOUND})

if(LIBSTORED_DOCUMENTATION)
	if(NOT DOXYGEN_FOUND)
		message(FATAL_ERROR "Doxygen is needed to build the documentation.")
	endif()

	add_custom_target(
		doc ALL
		COMMAND doxygen
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Generating API documentation with Doxygen (see doxygen/html/index.html)"
	)
endif()

if(WIN32)
	find_program(PYTHON_EXECUTABLE python)
else()
	find_program(PYTHON_EXECUTABLE python3)
	find_program(HAVE_VALGRIND valgrind)
endif()

if(NOT PYTHON_EXECUTABLE)
	message(FATAL_ERROR "Cannot find python")
else()
	message(STATUS "Using python ${PYTHON_EXECUTABLE}")
endif()

option(LIBSTORED_HAVE_LIBZMQ "Use libzmq" ON)

option(LIBSTORED_CPPCHECK "Enable cppcheck" OFF)
if(LIBSTORED_CPPCHECK)
	find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
	if(CMAKE_CXX_CPPCHECK)
		list(APPEND CMAKE_CXX_CPPCHECK
			"--enable=warning,style,information"
			"--force"
			"--inline-suppr"
#			"--quiet"
			"--suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/.cppcheck_suppr"
			"--error-exitcode=2"
#			"--xml"
		)
	endif()
endif()

add_subdirectory(extern)

set(LIBSTORED_SOURCE_DIR ${PROJECT_SOURCE_DIR} CACHE STRING "Path to the libstored sources" FORCE)

function(libstored_generate target) # add all other models as varargs
    foreach(model IN ITEMS ${ARGN})
        list(APPEND models ${CMAKE_CURRENT_SOURCE_DIR}/${model})
    endforeach()

    if(NOT EXISTS libstored)
        # Execute right away, such that the CMakeLists.txt exists.
        execute_process(
            COMMAND ${PYTHON_EXECUTABLE} ${LIBSTORED_SOURCE_DIR}/generator/generate.py -p ${target}- ${models} ${CMAKE_CURRENT_SOURCE_DIR}/libstored
        )
    endif()

    add_subdirectory(libstored)

    add_custom_command(
        OUTPUT ${target}-libstored.timestamp
        DEPENDS ${LIBSTORED_SOURCE_DIR}/include/libstored/store.h.tmpl
        DEPENDS ${LIBSTORED_SOURCE_DIR}/src/store.cpp.tmpl
        DEPENDS ${LIBSTORED_SOURCE_DIR}/CMakeLists.txt.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/generator/generate.py
		DEPENDS ${LIBSTORED_SOURCE_DIR}/generator/dsl/grammar.tx
		DEPENDS ${LIBSTORED_SOURCE_DIR}/generator/dsl/types.py
        DEPENDS ${models}
        COMMAND ${PYTHON_EXECUTABLE} ${LIBSTORED_SOURCE_DIR}/generator/generate.py -p ${target}- ${models} ${CMAKE_CURRENT_SOURCE_DIR}/libstored
        COMMAND ${CMAKE_COMMAND} -E touch ${target}-libstored.timestamp
        COMMENT "Generate store from ${ARGN}"
        VERBATIM
    )
    add_custom_target(${target}-libstored-generate
        DEPENDS ${target}-libstored.timestamp
    )
    add_dependencies(${target}-libstored ${target}-libstored-generate)

	get_target_property(target_type ${target} TYPE)
	if(target_type MATCHES "^(STATIC_LIBRARY|MODULE_LIBRARY|SHARED_LIBRARY|EXECUTABLE)$")
	    target_link_libraries(${target} PUBLIC ${target}-libstored)
	else()
		add_dependencies(${target} ${target}-libstored)
	endif()

    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
		if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			if(target_type STREQUAL "EXECUTABLE")
				target_link_options(${target} PUBLIC -Wl,--gc-sections)
			endif()
		endif()
    endif()

    if(LIBSTORED_DOCUMENTATION)
        add_dependencies(doc ${target}-libstored-generate)
    endif()
endfunction()

option(LIBSTORED_EXAMPLES "Build examples" ON)
if(LIBSTORED_EXAMPLES)
    add_subdirectory(examples)
endif()

add_subdirectory(client)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.10.0")
	option(LIBSTORED_TESTS "Build the tests" ON)
	if(LIBSTORED_TESTS)
		enable_testing()
		include(GoogleTest)
		add_subdirectory(tests)
	endif()
endif()

