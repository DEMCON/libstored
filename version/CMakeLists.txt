# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2023  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

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
