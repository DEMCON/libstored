# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MIT

cmake_policy(VERSION 3.10)

include(ExternalProject)
include(CheckIncludeFileCXX)

if(TARGET libzth)
	# Target already exists.
	set(Zth_FOUND 1)
	message(STATUS "Skipped looking for Zth; target already exists")
else()
	find_package(Zth CONFIG)
	if(Zth_FOUND)
		message(STATUS "Found Zth")
	endif()
endif()

if(NOT Zth_FOUND AND Zth_FIND_REQUIRED)
	message(STATUS "Cannot find installed Zth. Building from source.")

	if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		message(
			WARNING
				"Zth requires gcc, but you are using a different compiler. Cannot build from source."
		)
	else()
		set(Zth_FOUND 1)

		# CMAKE_MODULE_PATH and CMAKE_PREFIX_PATH may be ;-separated list. Passing ; via
		# ExternalProject_Add is a bit awkward...
		string(REPLACE ";" "\\\\\\\\\\\\\\\\;" CMAKE_MODULE_PATH_ "${CMAKE_MODULE_PATH}")
		string(REPLACE ";" "\\\\\\\\\\\\\\\\;" CMAKE_PREFIX_PATH_ "${CMAKE_PREFIX_PATH}")

		set(libzth_flags
		    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		    -DCMAKE_GENERATOR=${CMAKE_GENERATOR}
		    -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
		    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
		    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
		    -DZTH_BUILD_EXAMPLES=OFF
		    -DZTH_TESTS=OFF
		    -DZTH_DOCUMENTATION=OFF
		    -DZTH_THREADS=ON
		)

		if(LIBSTORED_DIST_DIR AND EXISTS ${LIBSTORED_DIST_DIR}/include)
			set(libzth_flags
			    ${libzth_flags}
			    -DZTH_PREPEND_INCLUDE_DIRECTORIES=${LIBSTORED_DIST_DIR}/include
			)
		endif()

		if(LIBSTORED_HAVE_LIBZMQ)
			set(libzth_flags ${libzth_flags} -DZTH_HAVE_LIBZMQ=ON)
		else()
			set(libzth_flags ${libzth_flags} -DZTH_HAVE_LIBZMQ=OFF)
		endif()

		if(LIBSTORED_ENABLE_ASAN)
			set(libzth_flags ${libzth_flags} -DZTH_ENABLE_ASAN=ON)
		endif()
		if(LIBSTORED_ENABLE_LSAN)
			set(libzth_flags ${libzth_flags} -DZTH_ENABLE_LSAN=ON)
		endif()
		if(LIBSTORED_ENABLE_UBSAN)
			set(libzth_flags ${libzth_flags} -DZTH_ENABLE_UBSAN=ON)
		endif()

		set(_libzth_loc ${CMAKE_INSTALL_PREFIX}/lib/libzth.a)

		set(libzth_repo "https://github.com/jhrutgers/zth.git")
		set(Zth_VERSION "1.1.0")
		set(libzth_tag "v${Zth_VERSION}")

		ExternalProject_Add(
			libzth-extern
			GIT_REPOSITORY ${libzth_repo}
			GIT_TAG ${libzth_tag}
			CMAKE_ARGS
				${libzth_flags}
				"-DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_}"
				"-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH_}"
				# Work-around for 1.1.0
				"-DCMAKE_CXX_FLAGS=-Wno-error=overloaded-virtual -Wno-error=mismatched-new-delete"
			INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
			BUILD_BYPRODUCTS ${_libzth_loc}
			UPDATE_DISCONNECTED 1
		)

		file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/include)
		file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)
		add_library(libzth STATIC IMPORTED GLOBAL)

		if(_libzth_loc)
			set_property(TARGET libzth PROPERTY IMPORTED_LOCATION ${_libzth_loc})
		endif()

		set_property(
			TARGET libzth PROPERTY INTERFACE_INCLUDE_DIRECTORIES
					       ${CMAKE_INSTALL_PREFIX}/include
		)
		add_dependencies(libzth libzth-extern)

		# It would be nicer to extract this information from the exported libzth.cmake file,
		# but ExternalProject installs it during the build stage, while we need the
		# information when configuring...
		target_compile_options(libzth INTERFACE -DZTH_THREADS=1)
		target_link_libraries(libzth INTERFACE pthread)
		if(UNIX)
			target_link_libraries(libzth INTERFACE rt dl)
		endif()
		if(NOT APPLE)
			check_include_file_cxx("libunwind.h" ZTH_HAVE_LIBUNWIND)
			if(ZTH_HAVE_LIBUNWIND)
				target_link_libraries(libzth INTERFACE unwind)
			endif()
		endif()

		if(LIBSTORED_HAVE_LIBZMQ)
			if(TARGET libzmq)
				# Make sure ZeroMQ is built and installed, before configuring Zth.
				# Otherwise, Zth may build it again.
				add_dependencies(libzth-extern libzmq)
			endif()
			target_compile_definitions(libzth INTERFACE ZTH_HAVE_LIBZMQ)
		endif()
	endif()
endif()
