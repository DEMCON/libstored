cmake_minimum_required(VERSION 3.1)
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

option(LIBSTORED_ENABLE_ASAN "Build with Address Sanitizer" OFF)
option(LIBSTORED_ENABLE_LSAN "Build with Leak Sanitizer" OFF)
option(LIBSTORED_ENABLE_UBSAN "Build with Undefined Behavior Sanitizer" OFF)

add_subdirectory(version)

option(LIBSTORED_DOCUMENTATION "Create the HTML based API documentation" ON)

if(LIBSTORED_DOCUMENTATION)
	add_subdirectory(sphinx)
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
option(LIBSTORED_HAVE_HEATSHRINK "Use heatshrink" ON)

add_subdirectory(extern)

set(LIBSTORED_SOURCE_DIR ${PROJECT_SOURCE_DIR} CACHE STRING "Path to the libstored sources" FORCE)

# Not safe against parallel execution if the target directory is used more than once.
function(libstored_copy_dlls target)
	if(WIN32 AND LIBSTORED_HAVE_LIBZMQ)
		get_target_property(target_type ${target} TYPE)
		if(target_type STREQUAL "EXECUTABLE")
			# Missing dll's... Really annoying. Just copy the libzmq.dll to wherever
			# it is possibly needed.
			if(CMAKE_STRIP)
				add_custom_command(TARGET ${target} PRE_LINK
					COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libzmq> $<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:libzmq>
					COMMAND ${CMAKE_STRIP} $<SHELL_PATH:$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:libzmq>>
					VERBATIM
				)
			else()
				add_custom_command(TARGET ${target} PRE_LINK
					COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libzmq> $<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:libzmq>
					VERBATIM
				)
			endif()
		endif()
	endif()
endfunction()

function(libstored_generate target) # add all other models as varargs
	foreach(model IN ITEMS ${ARGN})
		list(APPEND models ${CMAKE_CURRENT_SOURCE_DIR}/${model})
	endforeach()

	if(NOT EXISTS libstored)
		# Execute right away, such that the CMakeLists.txt exists.
		execute_process(
			COMMAND ${PYTHON_EXECUTABLE} ${LIBSTORED_SOURCE_DIR}/generator/generate.py -p ${target}- ${models} ${CMAKE_CURRENT_SOURCE_DIR}/libstored
			COMMAND ${CMAKE_COMMAND} -E touch ${target}-libstored.timestamp
		)
	endif()

	add_subdirectory(libstored)

	add_custom_command(
		OUTPUT ${target}-libstored.timestamp
		DEPENDS ${LIBSTORED_SOURCE_DIR}/include/libstored/store.h.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/src/store.cpp.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/doc/store.rtf.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/doc/store.csv.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/fpga/rtl/store.vhd.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/fpga/rtl/store_pkg.vhd.tmpl
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

	get_target_property(target_cxx_standard ${target} CXX_STANDARD)
	if(NOT target_cxx_standard STREQUAL "target_cxx_standard-NOTFOUND")
		set_target_properties(${target}-libstored PROPERTIES CXX_STANDARD ${target_cxx_standard})
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

	libstored_copy_dlls(${target})
endfunction()

if(DEFINED ENV{XILINX_VIVADO})
	find_program(VIVADO_CMD NAMES vivado PATHS "$ENV{XILINX_VIVADO}/bin")
endif()

if(NOT VIVADO_CMD)
	if(WIN32)
		file(GLOB VIVADO_CMDS "C:/Xilinx/Vivado/*/bin/vivado.bat")
	else()
		file(GLOB VIVADO_CMDS "/opt/Xilinx/Vivado/*/bin/vivado")
	endif()

	if(VIVADO_CMDS)
		list(SORT VIVADO_CMDS)
		list(GET VIVADO_CMDS -1 VIVADO_CMD)
	endif()
endif()

if(VIVADO_CMD)
	message(STATUS "Using vivado ${VIVADO_CMD}")
endif()

option(LIBSTORED_EXAMPLES "Build examples" ON)
if(LIBSTORED_EXAMPLES)
	add_subdirectory(examples)
endif()

option(LIBSTORED_PIP_INSTALL_USER "Run pip install --user when installing wheel files" OFF)
add_subdirectory(client)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.10.0")
	if(CMAKE_CROSSCOMPILING)
		set(LIBSTORED_TESTS_DEFAULT OFF)
	else()
		set(LIBSTORED_TESTS_DEFAULT ON)
	endif()

	option(LIBSTORED_TESTS "Build the tests" ${LIBSTORED_TESTS_DEFAULT})

	if(LIBSTORED_TESTS)
		enable_testing()

		if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.17.0")
			set(CTEST_OUTPUT_ON_FAILURE 1)
			list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")
		endif()

		include(GoogleTest)
		add_subdirectory(tests)
	endif()
endif()

find_program(CPPCHECK_CMD NAMES cppcheck)
if(CPPCHECK_CMD)
	if(LIBSTORED_EXAMPLES)
		set(CPPCHECK_EXAMPLES
			-I examples/1_hello/libstored/include
			-I examples/2_basic/libstored/include
			-I examples/3_scope/libstored/include
			-I examples/4_function/libstored/include
			-I examples/5_debug/libstored/include
			-I examples/6_hooks/libstored/include
			-I examples/7_protocol/libstored/include
			-I examples/8_sync/libstored/include
			-I examples/9_fpga/libstored/include
			-I examples/concurrency/libstored/include
			-I examples/terminal/libstored/include
			-I examples/zmqserver/libstored/include
			examples
		)
	endif()

	add_custom_target(libstored-cppcheck
		COMMAND
			${CPPCHECK_CMD}
			--enable=warning,style,information
			--force
			--inline-suppr
			--quiet
			--suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/.cppcheck_suppr
			--error-exitcode=1
#			--xml
			-I include
			${CPPCHECK_EXAMPLES}
			src
#			--check-config
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM
	)

	option(LIBSTORED_CPPCHECK "Run cppcheck by default" ON)
	if(LIBSTORED_CPPCHECK)
		add_custom_target(libstored-cppcheck-all ALL DEPENDS libstored-cppcheck)
	endif()
endif()

