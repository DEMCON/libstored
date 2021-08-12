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

if(TARGET libzmq)
	# Target already exists.
	set(ZeroMQ_FOUND 1)
	message(STATUS "Skipped looking for ZeroMQ; target already exists")
endif()

# However, we are using the zmq_poller draft API, which is not included by default. Should be fixed...
if(NOT TARGET libzmq)
	# Try pkg-config
	find_package(PkgConfig)

	if(PkgConfig_FOUND)
		pkg_check_modules(ZeroMQ libzmq>=4.3)

		if(ZeroMQ_FOUND)
			if(ZeroMQ_CFLAGS MATCHES "-DZMQ_BUILD_DRAFT_API=1")
				message(STATUS "Found ZeroMQ via pkg-config at ${ZeroMQ_LINK_LIBRARIES}")
				add_library(libzmq SHARED IMPORTED GLOBAL)
				set_property(TARGET libzmq PROPERTY IMPORTED_LOCATION ${ZeroMQ_LINK_LIBRARIES})
				target_include_directories(libzmq INTERFACE ${ZeroMQ_INCLUDE_DIRS})
				target_compile_options(libzmq INTERFACE ${ZeroMQ_CFLAGS})
				target_link_libraries(libzmq INTERFACE ${ZeroMQ_LDFLAGS})
			endif()
		endif()
	endif()
endif()

if(NOT TARGET libzmq)
	# Build from source
	message(STATUS "Building ZeroMQ from source")
	set(ZeroMQ_FOUND 1)

	set(libzmq_flags -DENABLE_DRAFTS=ON)

	if(MINGW)
		# See https://github.com/zeromq/libzmq/issues/3859
		set(libzmq_flags ${libzmq_flags} -DZMQ_CV_IMPL=win32api)
	endif()

	set(libzmq_flags ${libzmq_flags} -DBUILD_TESTS=OFF -DBUILD_STATIC=OFF)

	if(LIBSTORED_ENABLE_ASAN)
		set(libzmq_flags ${libzmq_flags} -DENABLE_ASAN=ON)
	endif()

	ExternalProject_Add(
		libzmq-extern
		GIT_REPOSITORY https://github.com/zeromq/libzmq.git
		GIT_TAG v4.3.2
		CMAKE_ARGS ${libzmq_flags} -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
		INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}
	)

	add_library(libzmq SHARED IMPORTED GLOBAL)
	if(WIN32)
		set_property(TARGET libzmq PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/lib/libzmq.dll)
		set_property(TARGET libzmq PROPERTY IMPORTED_IMPLIB ${CMAKE_CURRENT_BINARY_DIR}/lib/libzmq.lib)
	elseif(APPLE)
		set_property(TARGET libzmq PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/lib/libzmq.dylib)
	else()
		set_property(TARGET libzmq PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/lib/libzmq.a)
	endif()
	target_include_directories(libzmq INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include)
	target_compile_options(libzmq INTERFACE -DZMQ_BUILD_DRAFT_API=1)
endif()

