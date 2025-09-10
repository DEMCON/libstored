# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

cmake_policy(VERSION 3.10)

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/version.txt" LIBSTORED_VERSION LIMIT_COUNT 1)

message(STATUS "libstored version ${LIBSTORED_VERSION}")

string(REGEX REPLACE "^([0-9.]+).*$" "\\1" LIBSTORED_VERSION_BASE ${LIBSTORED_VERSION})
string(REPLACE "." ";" LIBSTORED_VERSION_LIST ${LIBSTORED_VERSION_BASE})
list(GET LIBSTORED_VERSION_LIST 0 LIBSTORED_VERSION_MAJOR)
list(GET LIBSTORED_VERSION_LIST 1 LIBSTORED_VERSION_MINOR)
list(GET LIBSTORED_VERSION_LIST 2 LIBSTORED_VERSION_PATCH)

if(LIBSTORED_VERSION_MAJOR GREATER "99"
   OR LIBSTORED_VERSION_MINOR GREATER "99"
   OR LIBSTORED_VERSION_PATCH GREATER "99"
)
	message(FATAL_ERROR "Please update version.h.in's STORED_VERSION_NUM")
endif()

string(REGEX REPLACE "^[0-9.]+(.*)$" "\\1" LIBSTORED_VERSION_SUFFIX ${LIBSTORED_VERSION})

set(LIBSTORED_VERSION_PYTHON ${LIBSTORED_VERSION_BASE})

if(NOT LIBSTORED_VERSION_SUFFIX STREQUAL "")
	string(APPEND LIBSTORED_VERSION_PYTHON ".dev0")
endif()

find_program(GIT_EXECUTABLE NAMES git)

if(GIT_EXECUTABLE)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} log -1 --format=%H
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		OUTPUT_VARIABLE LIBSTORED_VERSION_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
	)
endif()

# This file may be used while building libstored, so the version files should remain in sync.
# However, it is also possible that we only want to parse the version information, but the sources
# are read-only. Do not try to update the files in that case.

if("${CMAKE_PROJECT_NAME}" STREQUAL "libstored"
   OR NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/../include/libstored/version.h
)
	# Force reconfigure when version.txt is changed.
	configure_file(
		${CMAKE_CURRENT_LIST_DIR}/version.txt ${CMAKE_BINARY_DIR}/libstored-version.txt
		COPYONLY
	)

	configure_file(
		${CMAKE_CURRENT_LIST_DIR}/../include/libstored/version.h.in
		${CMAKE_CURRENT_LIST_DIR}/../include/libstored/version.h @ONLY ESCAPE_QUOTES
	)

	configure_file(
		${CMAKE_CURRENT_LIST_DIR}/../python/libstored/version.py.in
		${CMAKE_CURRENT_LIST_DIR}/../python/libstored/version.py @ONLY ESCAPE_QUOTES
	)

	configure_file(
		${CMAKE_CURRENT_LIST_DIR}/../python/version.txt.in
		${CMAKE_CURRENT_LIST_DIR}/../python/version.txt @ONLY ESCAPE_QUOTES
	)
endif()
