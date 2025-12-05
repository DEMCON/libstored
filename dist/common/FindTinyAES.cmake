# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MIT

cmake_policy(VERSION 3.10)

include(ExternalProject)
include(GNUInstallDirs)
find_package(Git)

if("${TINYAES_GIT_URL}" STREQUAL "")
	if(DEFINED ENV{LIBSTORED_GIT_CACHE})
		set(TINYAES_GIT_URL $ENV{LIBSTORED_GIT_CACHE}/tinyaes)
	else()
		set(TINYAES_GIT_URL "https://github.com/kokke/tiny-AES-c.git")
	endif()
endif()

set(TinyAES_VERSION 23856752fbd139da0b8ca6e471a13d5bcc99a08d)

ExternalProject_Add(
	tinyaes-extern
	GIT_REPOSITORY ${TINYAES_GIT_URL}
	GIT_TAG ${TinyAES_VERSION}
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
	UPDATE_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> ${GIT_EXECUTABLE} checkout -- .
	LOG_CONFIGURE 0
	LOG_BUILD 0
	LOG_TEST 0
	LOG_INSTALL 0
)

ExternalProject_Get_Property(tinyaes-extern SOURCE_DIR)

# The source files are considered generated files. Upon a clean, they are removed. Hence the
# UPDATE_COMMAND to recover them.
add_library(tinyaes STATIC ${SOURCE_DIR}/aes.c)
set_target_properties(tinyaes PROPERTIES PUBLIC_HEADER "${SOURCE_DIR}/aes.h")
add_dependencies(tinyaes tinyaes-extern)

get_target_property(tinyaes_src tinyaes SOURCES)
set_source_files_properties(${tinyaes_src} PROPERTIES GENERATED 1)

if(MSVC)
	target_compile_options(tinyaes PRIVATE /W1)
endif()

target_include_directories(
	tinyaes PUBLIC $<BUILD_INTERFACE:${SOURCE_DIR}> $<INSTALL_INTERFACE:include>
)

target_compile_definitions(tinyaes PUBLIC AES256=1 CTR=1 CBC=0 ECB=0)

install(
	TARGETS tinyaes
	EXPORT tinyaes
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(WIN32)
	install(EXPORT tinyaes DESTINATION CMake)
else()
	install(EXPORT tinyaes DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/libstored/cmake)
endif()

set(TinyAES_FOUND 1)
