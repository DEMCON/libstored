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

file(STRINGS "version.txt" LIBSTORED_VERSION LIMIT_COUNT 1)

message(STATUS "libstored version ${LIBSTORED_VERSION}")

string(REGEX REPLACE "^([0-9.]+).*$" "\\1" LIBSTORED_VERSION_BASE ${LIBSTORED_VERSION})
string(REPLACE "." ";" LIBSTORED_VERSION_LIST ${LIBSTORED_VERSION_BASE})
list(GET LIBSTORED_VERSION_LIST 0 LIBSTORED_VERSION_MAJOR)
list(GET LIBSTORED_VERSION_LIST 1 LIBSTORED_VERSION_MINOR)
list(GET LIBSTORED_VERSION_LIST 2 LIBSTORED_VERSION_PATCH)

if(	LIBSTORED_VERSION_MAJOR GREATER "99" OR
	LIBSTORED_VERSION_MINOR GREATER "99" OR
	LIBSTORED_VERSION_PATCH GREATER "99")
	message(FATAL_ERROR "Please update version.h.in's STORED_VERSION_NUM")
endif()

string(REGEX REPLACE "^[0-9.]+(.*)$" "\\1" LIBSTORED_VERSION_SUFFIX ${LIBSTORED_VERSION})

set(LIBSTORED_VERSION_PYTHON ${LIBSTORED_VERSION_BASE})

if(NOT LIBSTORED_VERSION_SUFFIX STREQUAL "")
	string(APPEND LIBSTORED_VERSION_PYTHON ".dev0")
endif()

# Force reconfigure when version.txt is changed.
configure_file(version.txt version.txt COPYONLY)

configure_file(
	${PROJECT_SOURCE_DIR}/include/libstored/version.h.in
	${PROJECT_SOURCE_DIR}/include/libstored/version.h
	@ONLY ESCAPE_QUOTES)

configure_file(
	${PROJECT_SOURCE_DIR}/python/libstored/version.py.in
	${PROJECT_SOURCE_DIR}/python/libstored/version.py
	@ONLY ESCAPE_QUOTES)

