# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

include(CheckIncludeFileCXX)

get_filename_component(libstored_dir_ "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
set(libstored_dir "${libstored_dir_}" CACHE INTERNAL "")

# Create the libstored library based on the generated files.
function(libstored_lib libprefix libpath)
	add_library(${libprefix}libstored STATIC
		${libstored_dir}/include/stored
		${libstored_dir}/include/stored.h
		${libstored_dir}/include/stored_config.h
		${libstored_dir}/include/libstored/allocator.h
		${libstored_dir}/include/libstored/compress.h
		${libstored_dir}/include/libstored/config.h
		${libstored_dir}/include/libstored/components.h
		${libstored_dir}/include/libstored/debugger.h
		${libstored_dir}/include/libstored/directory.h
		${libstored_dir}/include/libstored/macros.h
		${libstored_dir}/include/libstored/poller.h
		${libstored_dir}/include/libstored/spm.h
		${libstored_dir}/include/libstored/synchronizer.h
		${libstored_dir}/include/libstored/types.h
		${libstored_dir}/include/libstored/util.h
		${libstored_dir}/include/libstored/version.h
		${libstored_dir}/include/libstored/zmq.h
		${libstored_dir}/src/compress.cpp
		${libstored_dir}/src/directory.cpp
		${libstored_dir}/src/debugger.cpp
		${libstored_dir}/src/poller.cpp
		${libstored_dir}/src/protocol.cpp
		${libstored_dir}/src/synchronizer.cpp
		${libstored_dir}/src/util.cpp
		${libstored_dir}/src/zmq.cpp
	)

	foreach(m IN LISTS ARGN)
		target_sources(${libprefix}libstored PRIVATE
			${libpath}/include/${m}.h
			${libpath}/src/${m}.cpp)

		set_property(TARGET ${libprefix}libstored APPEND PROPERTY PUBLIC_HEADER ${libpath}/include/${m}.h)
		install(DIRECTORY ${libpath}/doc/ DESTINATION share/libstored)
	endforeach()

	target_include_directories(${libprefix}libstored PUBLIC
		$<BUILD_INTERFACE:${LIBSTORED_PREPEND_INCLUDE_DIRECTORIES}>
		$<BUILD_INTERFACE:${libstored_dir}/include>
		$<BUILD_INTERFACE:${libpath}/include>
		$<INSTALL_INTERFACE:include>
	)

	string(REGEX REPLACE "^(.*)-$" "stored-\\1" libname ${libprefix})
	set_target_properties(${libprefix}libstored PROPERTIES OUTPUT_NAME ${libname})
	target_compile_definitions(${libprefix}libstored PUBLIC -DSTORED_NAME=${libname})

	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		target_compile_definitions(${libprefix}libstored PUBLIC -D_DEBUG=1)
	else()
		target_compile_definitions(${libprefix}libstored PUBLIC -DNDEBUG=1)
	endif()

	if(MSVC)
		target_compile_options(${libprefix}libstored PRIVATE /Wall /WX)
	else()
		target_compile_options(${libprefix}libstored PRIVATE -Wall -Wextra -Werror -Wdouble-promotion -Wformat=2 -Wundef -Wconversion -ffunction-sections -fdata-sections)
	endif()
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		target_compile_options(${libprefix}libstored PRIVATE -Wno-defaulted-function-deleted)
	endif()

	if(LIBSTORED_DRAFT_API)
		target_compile_definitions(${libprefix}libstored PUBLIC -DSTORED_DRAFT_API=1)
	endif()

	CHECK_INCLUDE_FILE_CXX("valgrind/memcheck.h" LIBSTORED_HAVE_VALGRIND)
	if(LIBSTORED_HAVE_VALGRIND)
		target_compile_definitions(${libprefix}libstored PUBLIC -DSTORED_HAVE_VALGRIND=1)
	endif()

	if(TARGET libzth)
		message(STATUS "Enable Zth integration for ${libprefix}libstored")
		target_compile_definitions(${libprefix}libstored PUBLIC -DSTORED_HAVE_ZTH=1)
		target_link_libraries(${libprefix}libstored PUBLIC libzth)
	endif()

	if(LIBSTORED_HAVE_LIBZMQ)
		target_compile_definitions(${libprefix}libstored PUBLIC -DSTORED_HAVE_ZMQ=1)
		target_link_libraries(${libprefix}libstored PUBLIC libzmq)
	endif()

	if(WIN32)
		target_link_libraries(${libprefix}libstored INTERFACE ws2_32)
	endif()

	if(LIBSTORED_HAVE_HEATSHRINK)
		target_compile_definitions(${libprefix}libstored PUBLIC -DSTORED_HAVE_HEATSHRINK=1)
		target_link_libraries(${libprefix}libstored PUBLIC heatshrink)
	endif()

	if(${CMAKE_VERSION} VERSION_GREATER "3.6.0")
		find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")
		if(CLANG_TIDY_EXE AND LIBSTORED_CLANG_TIDY)
			message(STATUS "Enabled clang-tidy for ${libprefix}libstored")

			string(CONCAT CLANG_TIDY_CHECKS "-checks="
				"bugprone-*,"
				"-bugprone-macro-parentheses,"

				"clang-analyzer-*,"
				"concurrency-*,"

				"cppcoreguidelines-*,"
				"-cppcoreguidelines-avoid-c-arrays,"
				"-cppcoreguidelines-avoid-goto,"
				"-cppcoreguidelines-avoid-magic-numbers,"
				"-cppcoreguidelines-explicit-virtual-functions,"
				"-cppcoreguidelines-macro-usage,"
				"-cppcoreguidelines-pro-bounds-array-to-pointer-decay,"
				"-cppcoreguidelines-pro-bounds-pointer-arithmetic,"
				"-cppcoreguidelines-pro-type-union-access,"
				"-cppcoreguidelines-pro-type-vararg,"

				"hicpp-*,"
				"-hicpp-avoid-c-arrays,"
				"-hicpp-avoid-goto,"
				"-hicpp-braces-around-statements,"
				"-hicpp-member-init,"
				"-hicpp-no-array-decay,"
				"-hicpp-no-malloc,"
				"-hicpp-uppercase-literal-suffix,"
				"-hicpp-use-auto,"
				"-hicpp-use-override,"
				"-hicpp-vararg,"

				"misc-*,"
				"-misc-no-recursion,"
				"-misc-non-private-member-variables-in-classes,"
				"-misc-macro-parentheses,"

				"readability-*,"
				"-readability-braces-around-statements,"
				"-readability-convert-member-functions-to-static,"
				"-readability-else-after-return,"
				"-readability-function-cognitive-complexity,"
				"-readability-implicit-bool-conversion,"
				"-readability-magic-numbers,"
				"-readability-make-member-function-const,"
				"-readability-redundant-access-specifiers,"
				"-readability-uppercase-literal-suffix,"

				"performance-*,"
				"-performance-no-int-to-ptr," # Especially on WIN32 HANDLEs.

				"portability-*,"
			)
			set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" "${CLANG_TIDY_CHECKS}"
				"--extra-arg=-I${libstored_dir}/include"
				"--extra-arg=-I${CMAKE_BINARY_DIR}/include"
				"--extra-arg=-I${libpath}/include"
				"--header-filter=.*include/libstored.*"
				"--warnings-as-errors=*"
				"--extra-arg=-Wno-unknown-warning-option"
			)

			set_target_properties(${libprefix}libstored
				PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}" )
		else()
			set_target_properties(${libprefix}libstored
				PROPERTIES CXX_CLANG_TIDY "")
		endif()
	endif()

	if(LIBSTORED_ENABLE_ASAN)
		target_compile_options(${libprefix}libstored PRIVATE -fsanitize=address -fno-omit-frame-pointer)
		target_compile_definitions(${libprefix}libstored PRIVATE -DSTORED_ENABLE_ASAN=1)
		if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
			target_link_options(${libprefix}libstored INTERFACE -fsanitize=address)
		else()
			target_link_libraries(${libprefix}libstored INTERFACE "-fsanitize=address")
		endif()
	endif()

	if(LIBSTORED_ENABLE_LSAN)
		target_compile_options(${libprefix}libstored PRIVATE -fsanitize=leak -fno-omit-frame-pointer)
		target_compile_definitions(${libprefix}libstored PRIVATE -DSTORED_ENABLE_LSAN=1)
		if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
			target_link_options(${libprefix}libstored INTERFACE -fsanitize=leak)
		else()
			target_link_libraries(${libprefix}libstored INTERFACE "-fsanitize=leak")
		endif()
	endif()

	if(LIBSTORED_ENABLE_UBSAN)
		target_compile_options(${libprefix}libstored PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
		target_compile_definitions(${libprefix}libstored PRIVATE -DSTORED_ENABLE_UBSAN=1)
		if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
			target_link_options(${libprefix}libstored INTERFACE -fsanitize=undefined)
		else()
			target_link_libraries(${libprefix}libstored INTERFACE "-fsanitize=undefined")
		endif()
	endif()

	if(LIBSTORED_INSTALL_STORE_LIBS)
		install(TARGETS ${libprefix}libstored EXPORT libstored
			ARCHIVE DESTINATION lib
			PUBLIC_HEADER DESTINATION include
		)
	endif()
endfunction()

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

# Generate the store files and invoke libstored_lib to create the library for cmake.
function(libstored_generate target) # add all other models as varargs
	set(model_bases "")
	set(generated_files "")
	foreach(model IN ITEMS ${ARGN})
		list(APPEND models ${CMAKE_CURRENT_SOURCE_DIR}/${model})
		get_filename_component(model_base ${model} NAME_WE)
		list(APPEND model_bases ${model_base})
		list(APPEND generated_files ${CMAKE_CURRENT_SOURCE_DIR}/libstored/include/${model_base}.h)
		list(APPEND generated_files ${CMAKE_CURRENT_SOURCE_DIR}/libstored/src/${model_base}.cpp)
		list(APPEND generated_files ${CMAKE_CURRENT_SOURCE_DIR}/libstored/doc/${model_base}.rtf)
		list(APPEND generated_files ${CMAKE_CURRENT_SOURCE_DIR}/libstored/doc/${model_base}.csv)
		list(APPEND generated_files ${CMAKE_CURRENT_SOURCE_DIR}/libstored/rtl/${model_base}.vhd)
		list(APPEND generated_files ${CMAKE_CURRENT_SOURCE_DIR}/libstored/rtl/${model_base}_pkg.vhd)
	endforeach()

	add_custom_command(
		OUTPUT ${target}-libstored.timestamp ${generated_files}
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

	libstored_lib(${target}- ${CMAKE_CURRENT_SOURCE_DIR}/libstored ${model_bases})

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
		if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
			if(target_type STREQUAL "EXECUTABLE")
				target_link_options(${target} PUBLIC -Wl,--gc-sections)
			endif()
		endif()
	endif()

	if(LIBSTORED_DOCUMENTATION AND TARGET doc)
		add_dependencies(doc ${target}-libstored-generate)
	endif()

	libstored_copy_dlls(${target})
endfunction()

# libzth does not support installing yet...
if(NOT TARGET libzth)
	configure_file(${libstored_dir}/cmake/libstored.cmake.in ${CMAKE_BINARY_DIR}/libstored.cmake)
	install(DIRECTORY ${libstored_dir}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h")
	install(FILES ${libstored_dir}/include/stored DESTINATION include)
	install(EXPORT libstored DESTINATION share/libstored/cmake)
	install(FILES ${CMAKE_BINARY_DIR}/libstored.cmake DESTINATION share/cmake/libstored)
endif()

