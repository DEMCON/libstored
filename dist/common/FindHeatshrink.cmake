# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MIT

cmake_policy(VERSION 3.10)

include(ExternalProject)
include(GNUInstallDirs)
find_package(Git)

if("${HEATSHRINK_GIT_URL}" STREQUAL "")
	if(DEFINED ENV{LIBSTORED_GIT_CACHE})
		set(HEATSHRINK_GIT_URL $ENV{LIBSTORED_GIT_CACHE}/heatshrink)
	else()
		set(HEATSHRINK_GIT_URL "https://github.com/atomicobject/heatshrink.git")
	endif()
endif()

set(Heatshrink_VERSION 0.4.1)

ExternalProject_Add(
	heatshrink-extern
	GIT_REPOSITORY ${HEATSHRINK_GIT_URL}
	GIT_TAG v${Heatshrink_VERSION}
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
	UPDATE_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> ${GIT_EXECUTABLE} checkout -- .
	LOG_CONFIGURE 0
	LOG_BUILD 0
	LOG_TEST 0
	LOG_INSTALL 0
)

ExternalProject_Get_Property(heatshrink-extern SOURCE_DIR)

# heatshrink_encoder.c and heatshrink_decoder.c are considered generated files. Upon a clean, they
# are removed. Hence the UPDATE_COMMAND to recover them.
add_library(heatshrink STATIC ${SOURCE_DIR}/heatshrink_encoder.c ${SOURCE_DIR}/heatshrink_decoder.c)
set_target_properties(
	heatshrink
	PROPERTIES
		PUBLIC_HEADER
		"${SOURCE_DIR}/heatshrink_common.h;${SOURCE_DIR}/heatshrink_config.h;${SOURCE_DIR}/heatshrink_encoder.h;${SOURCE_DIR}/heatshrink_decoder.h"
)
add_dependencies(heatshrink heatshrink-extern)

get_target_property(heatshrink_src heatshrink SOURCES)
set_source_files_properties(${heatshrink_src} PROPERTIES GENERATED 1)

if(MSVC)
	target_compile_options(heatshrink PRIVATE /W1)
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		target_compile_options(heatshrink PUBLIC /MTd)
	else()
		target_compile_options(heatshrink PUBLIC /MT)
	endif()
endif()

target_include_directories(
	heatshrink PUBLIC $<BUILD_INTERFACE:${SOURCE_DIR}> $<INSTALL_INTERFACE:include>
)

install(
	TARGETS heatshrink
	EXPORT heatshrink
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(WIN32)
	install(EXPORT heatshrink DESTINATION CMake)
else()
	install(EXPORT heatshrink DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/libstored/cmake)
endif()

set(Heatshrink_FOUND 1)
