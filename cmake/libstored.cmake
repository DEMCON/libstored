# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

# ##################################################################################################
# Options

cmake_policy(VERSION 3.10)

option(LIBSTORED_INSTALL_STORE_LIBS "Install generated static libstored libraries" ON)
option(LIBSTORED_DRAFT_API "Enable draft API" ON)
option(LIBSTORED_DISABLE_EXCEPTIONS "Disable exception support" OFF)
option(LIBSTORED_DISABLE_RTTI "Disable run-time type information support" OFF)

# These options are related to development of libstored itself. By default, turn off.
option(LIBSTORED_DEV "Development related build options" OFF)
option(LIBSTORED_ENABLE_ASAN "Build with Address Sanitizer" OFF)
option(LIBSTORED_ENABLE_LSAN "Build with Leak Sanitizer" OFF)
option(LIBSTORED_ENABLE_UBSAN "Build with Undefined Behavior Sanitizer" OFF)
option(LIBSTORED_CLANG_TIDY "Run clang-tidy" OFF)
option(LIBSTORED_GCC_ANALYZER "Run GCC's analyzer" OFF)

# By default, only depend on other libraries when requested.

# When enabled, make sure there is a heatshrink CMake library target, or the staic library (with
# headers) is available in the normal search paths.
option(LIBSTORED_HAVE_HEATSHRINK "Use heatshrink" OFF)

# When enabled, a libzmq CMake static (imported) library target must exist, based on PkgConfig.
# Alternatively, ZeroMQ is built from source, which also provides the libzmq target.
option(LIBSTORED_HAVE_LIBZMQ "Use libzmq" OFF)

# When enabled, a libzth CMake library target must exist. Either via find_package(Zth), or built
# from source.
option(LIBSTORED_HAVE_ZTH "Use Zth" OFF)

# When enabled, the Qt5 or Qt6 namespace must exist, created via find_package().
option(LIBSTORED_HAVE_QT "Use Qt" OFF)

# ##################################################################################################
# Prepare environment

enable_language(CXX)
include(CheckIncludeFileCXX)
include(CMakeParseArguments)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(NOT LIBSTORED_SOURCE_DIR)
	get_filename_component(LIBSTORED_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
	set(LIBSTORED_SOURCE_DIR
	    "${LIBSTORED_SOURCE_DIR}"
	    CACHE INTERNAL ""
	)
endif()

if(NOT PYTHON_EXECUTABLE)
	if(CMAKE_HOST_WIN32)
		find_program(PYTHON_EXECUTABLE python REQUIRED)
	else()
		find_program(PYTHON_EXECUTABLE python3 REQUIRED)
	endif()
endif()

if(NOT EXISTS ${LIBSTORED_SOURCE_DIR}/python)
	# Execute from python package. PYTHONPATH is already fine.
	set(LIBSTORED_PYTHONPATH $ENV{PYTHONPATH})
elseif(WIN32)
	file(TO_NATIVE_PATH "${LIBSTORED_SOURCE_DIR}/python" LIBSTORED_PYTHONPATH)
	set(LIBSTORED_PYTHONPATH ${LIBSTORED_PYTHONPATH};$ENV{PYTHONPATH})
else()
	set(LIBSTORED_PYTHONPATH ${LIBSTORED_SOURCE_DIR}/python:$ENV{PYTHONPATH})
endif()

if(EXISTS ${LIBSTORED_SOURCE_DIR}/python)
	# Execute from git repo.
	set(LIBSTORED_GENERATOR_DIR "${LIBSTORED_SOURCE_DIR}/python/libstored/generator")
else()
	# Execute from python package.
	set(LIBSTORED_GENERATOR_DIR "${LIBSTORED_SOURCE_DIR}/../generator")
endif()

include(${LIBSTORED_SOURCE_DIR}/version/CMakeLists.txt)

# A dummy target that depends on all ...-generated targets.  May be handy in case of generating
# documentation, for example, where all generated header files are required.
add_custom_target(all-libstored-generate)

# ##################################################################################################
# libstored_*() functions.

# Create the libstored library based on the generated files.
#
# Old interface: libstored_lib(libprefix libpath store1 store2 ...)
#
# New interface: libstored_lib(TARGET lib DESTINATION libpath STORES store1 store1 ... [ZTH]
# [ZMQ|NO_ZMQ] [QT])
function(libstored_lib libprefix libpath)
	if("${libprefix}" STREQUAL "TARGET")
		cmake_parse_arguments(
			LIBSTORED_LIB "ZTH;ZMQ;NO_ZMQ;QT" "TARGET;DESTINATION" "STORES" ${ARGV}
		)
	else()
		message(DEPRECATION "Use keyword-based libstored_lib() instead.")
		set(LIBSTORED_LIB_TARGET ${libprefix}libstored)
		set(LIBSTORED_LIB_DESTINATION ${libpath})
		set(LIBSTORED_LIB_STORES ${ARGN})
	endif()

	# By default use ZMQ.
	set(LIBSTORED_LIB_ZMQ TRUE)

	if(LIBSTORED_LIB_NO_ZMQ)
		set(LIBSTORED_LIB_ZMQ FALSE)
	endif()

	set(LIBSTORED_LIB_TARGET_SRC "")
	set(LIBSTORED_LIB_TARGET_HEADERS "")
	foreach(m IN ITEMS ${LIBSTORED_LIB_STORES})
		list(APPEND LIBSTORED_LIB_TARGET_SRC "${LIBSTORED_LIB_DESTINATION}/include/${m}.h")
		list(APPEND LIBSTORED_LIB_TARGET_HEADERS
		     "${LIBSTORED_LIB_DESTINATION}/include/${m}.h"
		)
		list(APPEND LIBSTORED_LIB_TARGET_SRC "${LIBSTORED_LIB_DESTINATION}/src/${m}.cpp")
	endforeach()

	set(LIBSTORED_LIB_SBOM_CMAKE
	    "${CMAKE_CURRENT_BINARY_DIR}/${LIBSTORED_LIB_TARGET}-sbom.cmake"
	)

	if("${LIBSTORED_VERSION_HASH}" STREQUAL "")
		set(_version_hash)
	else()
		set(_version_hash " (git commit ${LIBSTORED_VERSION_HASH})")
	endif()

	# The namespace is the SHA1 hash over the doc/SHA1SUM file as UUID5 (truncated, with char
	# 13=5 and 21=8).
	file(
		WRITE "${LIBSTORED_LIB_SBOM_CMAKE}"
		"
		file(READ \"${LIBSTORED_LIB_DESTINATION}/doc/SHA1SUM\" _stores)
		file(SHA1 \"${LIBSTORED_LIB_DESTINATION}/doc/SHA1SUM\" _sha1)
		string(REGEX REPLACE \"^(........)(....).(...)(....).(...........).*$\" \"\\\\1-\\\\2-5\\\\3-\\\\4-8\\\\5\"
			_uuid5 \"\${_sha1}\")
		string(TIMESTAMP _now UTC)

		file(WRITE \"${LIBSTORED_LIB_DESTINATION}/doc/sbom.spdx\"
\"SPDXVersion: SPDX-2.3
DataLicense: CC0-1.0
SPDXID: SPDXRef-DOCUMENT
DocumentName: sbom
DocumentNamespace: https://github.com/DEMCON/libstored/spdxdocs/sbom-\${_uuid5}
Creator: Person: Anonymous ()
Creator: Organization: Anonymous ()
Creator: Tool: libstored-${LIBSTORED_VERSION}
Created: \${_now}
Relationship: SPDXRef-DOCUMENT DESCRIBES SPDXRef-libstored

PackageName: ${LIBSTORED_LIB_TARGET}
SPDXID: SPDXRef-libstored
PackageVersion: \${_sha1}
PackageDownloadLocation: NOASSERTION
FilesAnalyzed: false
PackageDescription: <text>Static library generated using libstored ${LIBSTORED_VERSION}${_version_hash},
CMake ${CMAKE_VERSION}, built with ${CMAKE_BUILD_TYPE} configuration for ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR}), containing:
\${_stores}</text>
PackageLicenseConcluded: MPL-2.0
PackageLicenseDeclared: MPL-2.0
PackageCopyrightText: <text>2020-2025 Jochem Rutgers</text>
ExternalRef: PACKAGE-MANAGER purl pkg:github/DEMCON/libstored@${LIBSTORED_VERSION_BASE}
PrimaryPackagePurpose: LIBRARY

PackageName: ${CMAKE_CXX_COMPILER_ID}
SPDXID: SPDXRef-compiler
PackageVersion: ${CMAKE_CXX_COMPILER_VERSION}
PackageDownloadLocation: NOASSERTION
FilesAnalyzed: false
PackageLicenseConcluded: NOASSERTION
PackageLicenseDeclared: NOASSERTION
PackageCopyrightText: NOASSERTION
PackageSummary: <text>The compiler as identified by CMake, running on ${CMAKE_HOST_SYSTEM_NAME} (${CMAKE_HOST_SYSTEM_PROCESSOR})</text>
PrimaryPackagePurpose: APPLICATION
Relationship: SPDXRef-compiler BUILD_DEPENDENCY_OF SPDXRef-libstored
\")
		"
	)

	add_custom_target(
		${LIBSTORED_LIB_TARGET}-sbom
		COMMAND ${CMAKE_COMMAND} -P "${LIBSTORED_LIB_SBOM_CMAKE}"
		VERBATIM
	)

	if(TARGET ${LIBSTORED_LIB_TARGET}-generate)
		add_dependencies(${LIBSTORED_LIB_TARGET}-sbom ${LIBSTORED_LIB_TARGET}-generate)
	endif()

	add_library(
		${LIBSTORED_LIB_TARGET} STATIC
		${LIBSTORED_SOURCE_DIR}/include/stored
		${LIBSTORED_SOURCE_DIR}/include/stored.h
		${LIBSTORED_SOURCE_DIR}/include/stored_config.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/allocator.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/compress.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/config.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/components.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/debugger.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/directory.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/macros.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/poller.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/spm.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/synchronizer.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/types.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/util.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/version.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/zmq.h
		${LIBSTORED_SOURCE_DIR}/src/compress.cpp
		${LIBSTORED_SOURCE_DIR}/src/directory.cpp
		${LIBSTORED_SOURCE_DIR}/src/debugger.cpp
		${LIBSTORED_SOURCE_DIR}/src/pipes.cpp
		${LIBSTORED_SOURCE_DIR}/src/poller.cpp
		${LIBSTORED_SOURCE_DIR}/src/protocol.cpp
		${LIBSTORED_SOURCE_DIR}/src/synchronizer.cpp
		${LIBSTORED_SOURCE_DIR}/src/util.cpp
		${LIBSTORED_SOURCE_DIR}/src/zmq.cpp
		${LIBSTORED_LIB_TARGET_SRC}
	)

	add_dependencies(${LIBSTORED_LIB_TARGET} ${LIBSTORED_LIB_TARGET}-sbom)

	target_include_directories(
		${LIBSTORED_LIB_TARGET}
		PUBLIC $<BUILD_INTERFACE:${LIBSTORED_PREPEND_INCLUDE_DIRECTORIES}>
		       $<BUILD_INTERFACE:${LIBSTORED_SOURCE_DIR}/include>
		       $<BUILD_INTERFACE:${LIBSTORED_LIB_DESTINATION}/include>
		       $<BUILD_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
		       $<INSTALL_INTERFACE:include>
	)

	string(REGEX REPLACE "^(.*)-libstored$" "stored-\\1" libname ${LIBSTORED_LIB_TARGET})
	set_target_properties(${LIBSTORED_LIB_TARGET} PROPERTIES OUTPUT_NAME ${libname})
	target_compile_definitions(${LIBSTORED_LIB_TARGET} PRIVATE -DSTORED_NAME=${libname})

	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -D_DEBUG=1)
	else()
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DNDEBUG=1)
	endif()

	if(MSVC)
		target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE /Wall)
		if(NOT (MSVC_VERSION LESS 1800) AND LIBSTORED_LIB_QT)
			target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE /wd4464)
		endif()
		if(LIBSTORED_DEV)
			target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE /WX)
		endif()
		if(LIBSTORED_DISABLE_RTTI)
			target_compile_options(${LIBSTORED_LIB_TARGET} PUBLIC /GR-)
		endif()
	else()
		target_compile_options(
			${LIBSTORED_LIB_TARGET}
			PRIVATE -Wall
				-Wextra
				-Wdouble-promotion
				-Wformat=2
				-Wundef
				-Wconversion
				-Wshadow
				-Wswitch-default
				-Wswitch-enum
				-Wfloat-equal
				-ffunction-sections
				-fdata-sections
		)
		if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE -Wlogical-op)
		endif()
		if(LIBSTORED_DEV)
			target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE -Werror)
			if(NOT WIN32)
				target_compile_options(
					${LIBSTORED_LIB_TARGET} PUBLIC -fstack-protector-strong
				)
			endif()
		endif()
		if(LIBSTORED_COVERAGE)
			target_compile_options(${LIBSTORED_LIB_TARGET} PUBLIC -O0 --coverage -pg)
			target_compile_definitions(
				${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_COVERAGE=1
			)
			target_link_libraries(${LIBSTORED_LIB_TARGET} PUBLIC "--coverage")
		endif()
		if(LIBSTORED_DISABLE_EXCEPTIONS)
			target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE -fno-exceptions)
		endif()
		if(LIBSTORED_DISABLE_RTTI)
			target_compile_options(${LIBSTORED_LIB_TARGET} PUBLIC -fno-rtti)
		endif()
	endif()
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER
						     "8.0.1"
	)
		# The flag should be there from LLVM 8.0.0, but I don't see it...
		target_compile_options(
			${LIBSTORED_LIB_TARGET} PUBLIC -Wno-defaulted-function-deleted
		)
	endif()

	if(LIBSTORED_DRAFT_API)
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_DRAFT_API=1)
	endif()

	check_include_file_cxx("valgrind/memcheck.h" LIBSTORED_HAVE_VALGRIND)
	if(LIBSTORED_HAVE_VALGRIND)
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_HAVE_VALGRIND=1)
	endif()

	if(LIBSTORED_HAVE_ZTH AND LIBSTORED_LIB_ZTH)
		message(STATUS "Enable Zth integration for ${LIBSTORED_LIB_TARGET}")
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_HAVE_ZTH=1)
		target_link_libraries(${LIBSTORED_LIB_TARGET} PUBLIC libzth)

		set(_fields)

		if("${Zth_VERSION}" STREQUAL "")
			set(_fields
			    "${_fields}
PackageVersion: preinstalled
PackageDownloadLocation: NOASSERTION
ExternalRef: PACKAGE-MANAGER purl pkg:github/jhrutgers/zth"
			)
		else()
			set(_fields
			    "${_fields}
PackageVersion: ${Zth_VERSION}
PackageDownloadLocation: https://github.com/jhrutgers/zth/releases/tag/v${Zth_VERSION}
ExternalRef: PACKAGE-MANAGER purl pkg:github/jhrutgers/zth@${Zth_VERSION}"
			)
		endif()

		file(
			APPEND "${LIBSTORED_LIB_SBOM_CMAKE}"
			"
			file(APPEND \"${LIBSTORED_LIB_DESTINATION}/doc/sbom.spdx\" \"
PackageName: Zth
SPDXID: SPDXRef-Zth${_fields}
PackageHomePage: https://github.com/jhrutgers/zth
FilesAnalyzed: false
PackageLicenseConcluded: MPL-2.0
PackageLicenseDeclared: MPL-2.0
PackageCopyrightText: 2019-2022 Jochem Rutgers
PackageSummary: <text>Cross-platform cooperative multitasking (fiber) framework.</text>
PrimaryPackagePurpose: LIBRARY
Relationship: SPDXRef-libstored DEPENDS_ON SPDXRef-Zth
\")
			"
		)
	endif()

	if(LIBSTORED_HAVE_LIBZMQ AND LIBSTORED_LIB_ZMQ)
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_HAVE_ZMQ=1)
		target_link_libraries(${LIBSTORED_LIB_TARGET} PUBLIC libzmq)

		set(_fields)

		if("${ZeroMQ_VERSION}" STREQUAL "")
			set(_fields
			    "${_fields}
PackageVersion: preinstalled
PackageDownloadLocation: NOASSERTION
ExternalRef: PACKAGE-MANAGER purl pkg:github/zeromq/libzmq"
			)
		else()
			set(_fields
			    "${_fields}
PackageVersion: ${ZeroMQ_VERSION}
PackageDownloadLocation: https://github.com/zeromq/libzmq/releases
ExternalRef: PACKAGE-MANAGER purl pkg:github/zeromq/libzmq@${ZeroMQ_VERSION}"
			)
		endif()

		file(
			APPEND "${LIBSTORED_LIB_SBOM_CMAKE}"
			"
			file(APPEND \"${LIBSTORED_LIB_DESTINATION}/doc/sbom.spdx\" \"
PackageName: libzmq
SPDXID: SPDXRef-libzmq${_fields}
PackageHomePage: https://zeromq.org/
FilesAnalyzed: false
PackageLicenseConcluded: MPL-2.0
PackageLicenseDeclared: MPL-2.0
PackageCopyrightText: NOASSERTION
PackageSummary: <text>A lightweight messaging kernel.</text>
PrimaryPackagePurpose: LIBRARY
Relationship: SPDXRef-libstored DEPENDS_ON SPDXRef-libzmq
\")
			"
		)
	endif()

	if(LIBSTORED_HAVE_QT AND LIBSTORED_LIB_QT)
		if(TARGET Qt5::Core)
			message(STATUS "Enable Qt5 integration for ${LIBSTORED_LIB_TARGET}")
			target_compile_definitions(
				${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_HAVE_QT=5
			)
			target_link_libraries(${LIBSTORED_LIB_TARGET} PUBLIC Qt5::Core)

			file(
				APPEND "${LIBSTORED_LIB_SBOM_CMAKE}"
				"
				file(APPEND \"${LIBSTORED_LIB_DESTINATION}/doc/sbom.spdx\" \"
PackageName: Qt5
SPDXID: SPDXRef-Qt5
PackageVersion: ${Qt5Core_VERSION}
PackageDownloadLocation: https://download.qt.io/
PackageHomePage: https://www.qt.io/
ExternalRef: PACKAGE-MANAGER purl pkg:github/qt/qt5@v${Qt5Core_VERSION}
FilesAnalyzed: false
PackageLicenseConcluded: NOASSERTION
PackageLicenseDeclared: LGPL-3.0-or-later
PackageCopyrightText: NOASSERTION
PrimaryPackagePurpose: LIBRARY
Relationship: SPDXRef-libstored DEPENDS_ON SPDXRef-Qt5
\")
				"
			)
		else()
			message(STATUS "Enable Qt6 integration for ${LIBSTORED_LIB_TARGET}")
			target_compile_definitions(
				${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_HAVE_QT=6
			)
			target_link_libraries(${LIBSTORED_LIB_TARGET} PUBLIC Qt::Core)

			file(
				APPEND "${LIBSTORED_LIB_SBOM_CMAKE}"
				"
				file(APPEND \"${LIBSTORED_LIB_DESTINATION}/doc/sbom.spdx\" \"
PackageName: Qt6
SPDXID: SPDXRef-Qt6
PackageVersion: ${Qt6Core_VERSION}
PackageDownloadLocation: https://download.qt.io/
PackageHomePage: https://www.qt.io/
ExternalRef: PACKAGE-MANAGER purl pkg:github/qt/qt5@v${Qt6Core_VERSION}
FilesAnalyzed: false
PackageLicenseConcluded: NOASSERTION
PackageLicenseDeclared: LGPL-3.0-or-later
PackageCopyrightText: NOASSERTION
PrimaryPackagePurpose: LIBRARY
Relationship: SPDXRef-libstored DEPENDS_ON SPDXRef-Qt6
\")
				"
			)
		endif()

		if(COMMAND qt_disable_unicode_defines)
			qt_disable_unicode_defines(${LIBSTORED_LIB_TARGET})
		endif()

		set_target_properties(${LIBSTORED_LIB_TARGET} PROPERTIES AUTOMOC ON)
	endif()

	if(WIN32)
		target_link_libraries(${LIBSTORED_LIB_TARGET} INTERFACE ws2_32)
	endif()

	if(LIBSTORED_HAVE_HEATSHRINK)
		target_compile_definitions(
			${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_HAVE_HEATSHRINK=1
		)
		target_link_libraries(${LIBSTORED_LIB_TARGET} PUBLIC heatshrink)

		set(_fields)

		if("${Heatshrink_VERSION}" STREQUAL "")
			set(_fields
			    "${_fields}
PackageVersion: preinstalled
PackageDownloadLocation: NOASSERTION
ExternalRef: PACKAGE-MANAGER purl pkg:github/atomicobject/heatshrink"
			)
		else()
			set(_fields
			    "${_fields}
PackageVersion: ${Heatshrink_VERSION}
PackageDownloadLocation: https://github.com/atomicobject/heatshrink/releases/tag/v${Heatshrink_VERSION}
ExternalRef: PACKAGE-MANAGER purl pkg:github/atomicobject/heatshrink@${Heatshrink_VERSION}"
			)
		endif()

		file(
			APPEND "${LIBSTORED_LIB_SBOM_CMAKE}"
			"
			file(APPEND \"${LIBSTORED_LIB_DESTINATION}/doc/sbom.spdx\" \"
PackageName: heatshrink
SPDXID: SPDXRef-heatshrink${_fields}
PackageHomePage: https://github.com/atomicobject/heatshrink
FilesAnalyzed: false
PackageLicenseConcluded: ISC
PackageLicenseDeclared: ISC
PackageCopyrightText: 2013-2015, Scott Vokes <vokes.s@gmail.com>
PackageSummary: <text>A data compression/decompression library for embedded/real-time systems.</text>
PrimaryPackagePurpose: LIBRARY
Relationship: SPDXRef-libstored DEPENDS_ON SPDXRef-heatshrink
\")
			"
		)
	endif()

	set(DO_CLANG_TIDY "")

	if(${CMAKE_VERSION} VERSION_GREATER "3.6.0")
		find_program(
			CLANG_EXE
			NAMES "clang"
			DOC "Path to clang executable"
		)

		if(CLANG_EXE AND NOT CLANG_TIDY_EXE14)
			execute_process(
				COMMAND ${CLANG_EXE} -dumpversion
				OUTPUT_VARIABLE CLANG_VERSION
				ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
			)

			# We need clang-tidy 14 or later for --config-file.
			if("${CLANG_VERSION}" VERSION_GREATER_EQUAL 14)
				find_program(
					CLANG_TIDY_EXE14
					NAMES "clang-tidy"
					DOC "Path to clang-tidy executable"
				)

				if(CLANG_TIDY_EXE14)
					message(STATUS "Found clang-tidy ${CLANG_VERSION}")
				endif()
			endif()
		endif()

		if(CLANG_TIDY_EXE14 AND LIBSTORED_CLANG_TIDY)
			message(STATUS "Enabled clang-tidy for ${LIBSTORED_LIB_TARGET}")

			set(DO_CLANG_TIDY
			    "${CLANG_TIDY_EXE14}"
			    "--config-file=${LIBSTORED_SOURCE_DIR}/.clang-tidy"
			    "--extra-arg=-I${LIBSTORED_SOURCE_DIR}/include"
			    "--extra-arg=-I${LIBSTORED_LIB_DESTINATION}/include"
			)

			if(CMAKE_CXX_STANDARD)
				set(DO_CLANG_TIDY "${DO_CLANG_TIDY}"
						  "--extra-arg=-std=c++${CMAKE_CXX_STANDARD}"
				)
			endif()

			set_target_properties(
				${LIBSTORED_LIB_TARGET} PROPERTIES CXX_CLANG_TIDY
								   "${DO_CLANG_TIDY}"
			)
		else()
			set_target_properties(${LIBSTORED_LIB_TARGET} PROPERTIES CXX_CLANG_TIDY "")
		endif()
	endif()

	if(LIBSTORED_GCC_ANALYZER AND "${DO_CLANG_TIDY}" STREQUAL "")
		target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE -fanalyzer)
	endif()

	if(LIBSTORED_ENABLE_ASAN)
		target_compile_options(
			${LIBSTORED_LIB_TARGET} PUBLIC -fsanitize=address -fno-omit-frame-pointer
		)
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_ENABLE_ASAN=1)
		if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
			target_link_options(${LIBSTORED_LIB_TARGET} INTERFACE -fsanitize=address)
		else()
			target_link_libraries(
				${LIBSTORED_LIB_TARGET} INTERFACE "-fsanitize=address"
			)
		endif()
	endif()

	if(LIBSTORED_ENABLE_LSAN)
		target_compile_options(
			${LIBSTORED_LIB_TARGET} PUBLIC -fsanitize=leak -fno-omit-frame-pointer
		)
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_ENABLE_LSAN=1)
		if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
			target_link_options(${LIBSTORED_LIB_TARGET} INTERFACE -fsanitize=leak)
		else()
			target_link_libraries(${LIBSTORED_LIB_TARGET} INTERFACE "-fsanitize=leak")
		endif()
	endif()

	if(LIBSTORED_ENABLE_UBSAN)
		target_compile_options(
			${LIBSTORED_LIB_TARGET} PUBLIC -fsanitize=undefined -fno-omit-frame-pointer
		)

		if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			target_compile_options(
				${LIBSTORED_LIB_TARGET}
				PUBLIC # The combination of -fno-sanitize-recover and ubsan gives
				       # some issues with vptr. This might be related:
				       # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=94325. Disable
				       # it for now.
				       -fno-sanitize=vptr
				       # The following checks seem to crash the compiler (at least
				       # with GCC 11), espeically on the components example (heavily
				       # using libstored/components.h).
				       -fno-sanitize=null
				       -fno-sanitize=nonnull-attribute
				       -fno-sanitize=returns-nonnull-attribute
			)
		endif()

		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_ENABLE_UBSAN=1)
		if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
			target_link_options(${LIBSTORED_LIB_TARGET} INTERFACE -fsanitize=undefined)
		else()
			target_link_libraries(
				${LIBSTORED_LIB_TARGET} INTERFACE "-fsanitize=undefined"
			)
		endif()
	endif()

	if(LIBSTORED_INSTALL_STORE_LIBS)
		install(
			TARGETS ${LIBSTORED_LIB_TARGET}
			EXPORT ${LIBSTORED_LIB_TARGET}Store
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
		)

		install(FILES ${LIBSTORED_LIB_TARGET_HEADERS}
			DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
		)

		install(DIRECTORY ${LIBSTORED_LIB_DESTINATION}/doc/
			DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/libstored
		)

		if(WIN32)
			install(EXPORT ${LIBSTORED_LIB_TARGET}Store DESTINATION CMake)
		else()
			install(EXPORT ${LIBSTORED_LIB_TARGET}Store
				DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/libstored/cmake
			)
		endif()
	endif()
endfunction()

# Not safe against parallel execution if the target directory is used more than once.
function(libstored_copy_dlls target)
	if(WIN32 AND LIBSTORED_HAVE_LIBZMQ)
		get_target_property(target_type ${target} TYPE)
		if(target_type STREQUAL "EXECUTABLE")
			# Missing dll's... Really annoying. Just copy the libzmq.dll to wherever it
			# is possibly needed.
			if(CMAKE_STRIP)
				add_custom_command(
					TARGET ${target}
					PRE_LINK
					COMMAND
						${CMAKE_COMMAND} -E copy $<TARGET_FILE:libzmq>
						$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:libzmq>
					COMMAND
						${CMAKE_STRIP}
						$<SHELL_PATH:$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:libzmq>>
					VERBATIM
				)
			else()
				add_custom_command(
					TARGET ${target}
					PRE_LINK
					COMMAND
						${CMAKE_COMMAND} -E copy $<TARGET_FILE:libzmq>
						$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:libzmq>
					VERBATIM
				)
			endif()
		endif()
	endif()
endfunction()

# Generate the store files and invoke libstored_lib to create the library for cmake.
#
# Old interface: libstored_generate(target store1 store2 ...)
#
# New interface: libstored_generate(TARGET target [DESTINATION path] STORES store1 store2 [ZTH]
# [ZMQ|NO_ZMQ])
function(libstored_generate target)
	if("${target}" STREQUAL "TARGET")
		cmake_parse_arguments(
			LIBSTORED_GENERATE "ZTH;ZMQ;NO_ZMQ;QT" "TARGET;DESTINATION" "STORES"
			${ARGV}
		)
	else()
		message(DEPRECATION "Use keyword-based libstored_generate() instead.")
		set(LIBSTORED_GENERATE_TARGET ${target})
		set(LIBSTORED_GENERATE_STORES ${ARGN})
	endif()

	set(LIBSTORED_GENERATE_FLAGS)
	if(LIBSTORED_GENERATE_ZTH)
		set(LIBSTORED_GENERATE_FLAGS ${LIBSTORED_GENERATE_FLAGS} ZTH)
	endif()
	if(LIBSTORED_GENERATE_ZMQ)
		set(LIBSTORED_GENERATE_FLAGS ${LIBSTORED_GENERATE_FLAGS} ZMQ)
	endif()
	if(LIBSTORED_GENERATE_NO_ZMQ)
		set(LIBSTORED_GENERATE_FLAGS ${LIBSTORED_GENERATE_FLAGS} NO_ZMQ)
	endif()
	if(LIBSTORED_GENERATE_QT)
		set(LIBSTORED_GENERATE_FLAGS ${LIBSTORED_GENERATE_FLAGS} QT)
	endif()

	if("${LIBSTORED_GENERATE_DESTINATION}" STREQUAL "")
		set(LIBSTORED_GENERATE_DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/libstored)
	endif()

	set(model_bases "")
	set(generated_files
	    ${LIBSTORED_GENERATE_DESTINATION}/CMakeLists.txt
	    ${LIBSTORED_GENERATE_DESTINATION}/rtl/vivado.tcl
	    ${LIBSTORED_GENERATE_DESTINATION}/doc/libstored-src.spdx
	    ${LIBSTORED_GENERATE_DESTINATION}/doc/SHA1SUM
	)
	foreach(model IN ITEMS ${LIBSTORED_GENERATE_STORES})
		get_filename_component(model_abs "${model}" ABSOLUTE)
		list(APPEND models ${model_abs})
		get_filename_component(model_base "${model}" NAME_WE)
		list(APPEND model_bases ${model_base})
		list(APPEND generated_files
		     ${LIBSTORED_GENERATE_DESTINATION}/include/${model_base}.h
		)
		list(APPEND generated_files ${LIBSTORED_GENERATE_DESTINATION}/src/${model_base}.cpp)
		list(APPEND generated_files ${LIBSTORED_GENERATE_DESTINATION}/doc/${model_base}.rtf)
		list(APPEND generated_files ${LIBSTORED_GENERATE_DESTINATION}/doc/${model_base}.csv)
		list(APPEND generated_files
		     ${LIBSTORED_GENERATE_DESTINATION}/doc/${model_base}Meta.py
		)
		list(APPEND generated_files ${LIBSTORED_GENERATE_DESTINATION}/rtl/${model_base}.vhd)
		list(APPEND generated_files
		     ${LIBSTORED_GENERATE_DESTINATION}/rtl/${model_base}_pkg.vhd
		)
	endforeach()

	add_custom_command(
		OUTPUT ${LIBSTORED_GENERATE_TARGET}-libstored.timestamp ${generated_files}
		DEPENDS ${LIBSTORED_SOURCE_DIR}/include/libstored/store.h.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/src/store.cpp.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/doc/libstored-src.spdx.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/doc/SHA1SUM.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/doc/store.rtf.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/doc/store.csv.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/doc/store.py.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/fpga/rtl/store.vhd.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/fpga/rtl/store_pkg.vhd.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/CMakeLists.txt.tmpl
		DEPENDS ${LIBSTORED_GENERATOR_DIR}/__main__.py
		DEPENDS ${LIBSTORED_GENERATOR_DIR}/dsl/grammar.tx
		DEPENDS ${LIBSTORED_GENERATOR_DIR}/dsl/types.py
		DEPENDS ${models}
		COMMAND
			${CMAKE_COMMAND} -E env PYTHONPATH=${LIBSTORED_PYTHONPATH}
			${PYTHON_EXECUTABLE} -m libstored.generator -p ${LIBSTORED_GENERATE_TARGET}-
			${models} ${LIBSTORED_GENERATE_DESTINATION}
		COMMAND ${CMAKE_COMMAND} -E touch ${LIBSTORED_GENERATE_TARGET}-libstored.timestamp
		COMMENT "Generate store from ${LIBSTORED_GENERATE_STORES}"
		VERBATIM
	)
	add_custom_target(
		${LIBSTORED_GENERATE_TARGET}-libstored-generate
		DEPENDS ${LIBSTORED_GENERATE_TARGET}-libstored.timestamp
	)
	add_dependencies(all-libstored-generate ${LIBSTORED_GENERATE_TARGET}-libstored-generate)

	libstored_lib(
		TARGET
		${LIBSTORED_GENERATE_TARGET}-libstored
		DESTINATION
		${LIBSTORED_GENERATE_DESTINATION}
		STORES
		${model_bases}
		${LIBSTORED_GENERATE_FLAGS}
	)

	add_dependencies(
		${LIBSTORED_GENERATE_TARGET}-libstored
		${LIBSTORED_GENERATE_TARGET}-libstored-generate
	)

	get_target_property(target_type ${LIBSTORED_GENERATE_TARGET} TYPE)
	if(target_type MATCHES "^(STATIC_LIBRARY|MODULE_LIBRARY|SHARED_LIBRARY|EXECUTABLE)$")
		target_link_libraries(
			${LIBSTORED_GENERATE_TARGET} PUBLIC ${LIBSTORED_GENERATE_TARGET}-libstored
		)
	else()
		add_dependencies(
			${LIBSTORED_GENERATE_TARGET} ${LIBSTORED_GENERATE_TARGET}-libstored
		)
	endif()

	get_target_property(target_cxx_standard ${LIBSTORED_GENERATE_TARGET} CXX_STANDARD)
	if(target_cxx_standard)
		set_target_properties(
			${LIBSTORED_GENERATE_TARGET}-libstored PROPERTIES CXX_STANDARD
									  "${target_cxx_standard}"
		)
	endif()

	if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
		if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
			if(target_type STREQUAL "EXECUTABLE")
				target_link_options(
					${LIBSTORED_GENERATE_TARGET} PUBLIC -Wl,--gc-sections
				)
			endif()
		elseif(APPLE)
			if(target_type STREQUAL "EXECUTABLE")
				target_link_options(
					${LIBSTORED_GENERATE_TARGET} PUBLIC -Wl,-dead_strip
				)
			endif()
		endif()
	endif()

	foreach(d IN LISTS LIBSTORED_GENERATE_DEPS)
		add_dependencies(${d} ${LIBSTORED_GENERATE_TARGET}-libstored-generate)
	endforeach()

	libstored_copy_dlls(${LIBSTORED_GENERATE_TARGET})
endfunction()

find_program(RCC_EXE pyside6-rcc PATHS $ENV{HOME}/.local/bin)

cmake_policy(SET CMP0058 NEW)

if(NOT RCC_EXE STREQUAL "RCC_EXE-NOTFOUND")
	function(libstored_visu target rcc)
		foreach(f IN LISTS ARGN)
			get_filename_component(f_abs ${f} ABSOLUTE)

			if(f_abs MATCHES "^(.*/)?main\\.qml$")
				set(qrc_main "${f_abs}")
				string(REGEX REPLACE "^(.*/)?main.qml$" "\\1" qrc_prefix ${f_abs})
			endif()
		endforeach()

		if(NOT qrc_main)
			message(FATAL_ERROR "Missing main.qml input for ${target}")
		endif()

		string(LENGTH "${qrc_prefix}" qrc_prefix_len)

		# REUSE-IgnoreStart
		set(qrc
		    "<!--
SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers

SPDX-License-Identifier: MPL-2.0
-->

<!DOCTYPE RCC>
<RCC version=\"1.0\">
<qresource>
"
		)
		# REUSE-IgnoreEnd
		foreach(f IN LISTS ARGN)
			get_filename_component(f_abs ${f} ABSOLUTE)
			if(qrc_prefix_len GREATER 0)
				string(SUBSTRING "${f_abs}" 0 ${qrc_prefix_len} f_prefix)
				if(f_prefix STREQUAL qrc_prefix)
					string(SUBSTRING "${f_abs}" ${qrc_prefix_len} -1 f_alias)
					set(qrc
					    "${qrc}<file alias=\"${f_alias}\">${f_abs}</file>\n"
					)
				else()
					set(qrc "${qrc}<file>${f_abs}</file>\n")
				endif()
			else()
				set(qrc "${qrc}<file>${f_abs}</file>\n")
			endif()
		endforeach()
		set(qrc "${qrc}</qresource>\n</RCC>\n")

		get_filename_component(rcc ${rcc} ABSOLUTE)
		file(
			GENERATE
			OUTPUT ${rcc}.qrc
			CONTENT "${qrc}"
		)

		add_custom_command(
			OUTPUT ${rcc}
			DEPENDS
				${LIBSTORED_SOURCE_DIR}/python/libstored/visu/visu.qrc
				${LIBSTORED_SOURCE_DIR}/python/libstored/visu/qml/Libstored/Components/Input.qml
				${LIBSTORED_SOURCE_DIR}/python/libstored/visu/qml/Libstored/Components/Measurement.qml
				${LIBSTORED_SOURCE_DIR}/python/libstored/visu/qml/Libstored/Components/StoreObject.qml
				${LIBSTORED_SOURCE_DIR}/python/libstored/visu/qml/Libstored/Components/qmldir
				${ARGN}
				${rcc}.qrc
			COMMAND
				${RCC_EXE}
				$<SHELL_PATH:${LIBSTORED_SOURCE_DIR}/python/libstored/visu/visu.qrc>
				$<SHELL_PATH:${rcc}.qrc> -o $<SHELL_PATH:${rcc}>
			COMMENT "Generating ${target} visu"
			VERBATIM
		)

		set_property(
			SOURCE ${rcc}.qrc ${LIBSTORED_SOURCE_DIR}/python/libstored/visu/visu.qrc
			PROPERTY AUTORCC OFF
		)
		add_custom_target(
			${target}
			DEPENDS ${rcc}
			SOURCES ${rcc}.qrc ${LIBSTORED_SOURCE_DIR}/python/libstored/visu/visu.qrc
		)
	endfunction()
endif()

if(LIBSTORED_INSTALL_STORE_LIBS)
	install(
		DIRECTORY ${LIBSTORED_SOURCE_DIR}/include/libstored/
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libstored
		FILES_MATCHING
		PATTERN "*.h"
	)
	install(FILES ${LIBSTORED_SOURCE_DIR}/include/stored
		      ${LIBSTORED_SOURCE_DIR}/include/stored.h
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	)

	set(LIBSTORED_CONFIG_FILE ${LIBSTORED_SOURCE_DIR}/include/stored_config.h)
	foreach(d IN LISTS LIBSTORED_PREPEND_INCLUDE_DIRECTORIES)
		if(EXISTS ${d}/stored_config.h)
			set(LIBSTORED_CONFIG_FILE ${d}/stored_config.h)
			break()
		endif()
	endforeach()
	install(FILES ${LIBSTORED_CONFIG_FILE} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

	if(WIN32)
		set(LIBSTORED_STORES_CMAKE_PATH CMake)
	else()
		set(LIBSTORED_STORES_CMAKE_PATH ${CMAKE_INSTALL_DATAROOTDIR}/cmake/LibstoredStores)
	endif()

	configure_package_config_file(
		"${LIBSTORED_SOURCE_DIR}/cmake/LibstoredStoresConfig.cmake.in"
		"${PROJECT_BINARY_DIR}/LibstoredStoresConfig.cmake"
		INSTALL_DESTINATION ${LIBSTORED_STORES_CMAKE_PATH}
	)

	install(FILES ${PROJECT_BINARY_DIR}/LibstoredStoresConfig.cmake
		DESTINATION ${LIBSTORED_STORES_CMAKE_PATH}
	)
endif()
