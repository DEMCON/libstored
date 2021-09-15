# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2021  Jochem Rutgers
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

include(ExternalProject)

if(TARGET libzmq)
	# Target already exists.
	set(ZeroMQ_FOUND 1)
	message(STATUS "Skipped looking for ZeroMQ; target already exists")
endif()

# However, we are using the zmq_poller draft API, which is not included by default. Should be fixed...
if(NOT TARGET libzmq AND NOT CMAKE_CROSSCOMPILING)
	# Try pkg-config
	find_package(PkgConfig)

	if(PkgConfig_FOUND)
		pkg_check_modules(ZeroMQ libzmq>=4.3 IMPORTED_TARGET)

		if(ZeroMQ_FOUND)
			if(NOT ZeroMQ_LINK_LIBRARIES)
				set(ZeroMQ_LINK_LIBRARIES ${pkgcfg_lib_ZeroMQ_zmq})
			endif()
			if(ZeroMQ_LINK_LIBRARIES AND ZeroMQ_CFLAGS MATCHES "-DZMQ_BUILD_DRAFT_API=1")
				message(STATUS "Found ZeroMQ via pkg-config at ${ZeroMQ_LINK_LIBRARIES}")
				add_library(libzmq SHARED IMPORTED GLOBAL)
				set_property(TARGET libzmq PROPERTY IMPORTED_LOCATION ${ZeroMQ_LINK_LIBRARIES})
				set_property(TARGET libzmq PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${ZeroMQ_INCLUDE_DIRS})
				set_property(TARGET libzmq PROPERTY INTERFACE_COMPILE_OPTIONS ${ZeroMQ_CFLAGS})
				target_link_libraries(libzmq INTERFACE ${ZeroMQ_LDFLAGS})
			endif()
		endif()
	endif()
endif()

if(NOT TARGET libzmq)
	# Build from source
	message(STATUS "Building ZeroMQ from source")
	set(ZeroMQ_FOUND 1)

	set(libzmq_flags -DENABLE_DRAFTS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_GENERATOR=${CMAKE_GENERATOR}
		-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
	)

	if(MINGW)
		# See https://github.com/zeromq/libzmq/issues/3859
		set(libzmq_flags ${libzmq_flags} -DZMQ_CV_IMPL=win32api)
	endif()

	set(libzmq_flags ${libzmq_flags} -DBUILD_TESTS=OFF -DBUILD_STATIC=OFF)

	if(LIBSTORED_ENABLE_ASAN)
		set(libzmq_flags ${libzmq_flags} -DENABLE_ASAN=ON)
	endif()

	if(WIN32)
		if(MSVC)
			if(MSVC_IDE)
				set(MSVC_TOOLSET "-${CMAKE_VS_PLATFORM_TOOLSET}")
			else()
				set(MSVC_TOOLSET "")
			endif()
			if(CMAKE_BUILD_TYPE STREQUAL "Debug")
				set(dllname "${MSVC_TOOLSET}-mt-gd")
			else()
				set(dllname "${MSVC_TOOLSET}-mt")
			endif()
			set(_libzmq_loc ${CMAKE_INSTALL_PREFIX}/bin/libzmq${dllname}-4_3_1.dll)
			set(_libzmq_implib ${CMAKE_INSTALL_PREFIX}/lib/libzmq${dllname}-4_3_1.lib)
		else()
			set(_libzmq_loc ${CMAKE_INSTALL_PREFIX}/bin/libzmq.dll)
			set(_libzmq_implib ${CMAKE_INSTALL_PREFIX}/lib/libzmq.dll.a)
		endif()
	elseif(APPLE)
		set(_libzmq_loc ${CMAKE_INSTALL_PREFIX}/lib/libzmq.dylib)
	else()
		set(_libzmq_loc ${CMAKE_INSTALL_PREFIX}/lib/libzmq.so)
	endif()

	ExternalProject_Add(
		libzmq-extern
		GIT_REPOSITORY https://github.com/zeromq/libzmq.git
		GIT_TAG v4.3.1
		CMAKE_ARGS ${libzmq_flags}
		INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
		BUILD_BYPRODUCTS ${_libzmq_loc} ${_libzmq_implib}
		UPDATE_DISCONNECTED 1
	)

	file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/include)
	file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)

	add_library(libzmq SHARED IMPORTED GLOBAL)

	if(_libzmq_loc)
		set_property(TARGET libzmq PROPERTY IMPORTED_LOCATION ${_libzmq_loc})
	endif()
	if(_libzmq_implib)
		set_property(TARGET libzmq PROPERTY IMPORTED_IMPLIB ${_libzmq_implib})
	endif()

	set_property(TARGET libzmq PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_INSTALL_PREFIX}/include)
	target_compile_options(libzmq INTERFACE -DZMQ_BUILD_DRAFT_API=1)
	add_dependencies(libzmq libzmq-extern)
endif()

