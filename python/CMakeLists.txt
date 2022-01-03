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

list(APPEND pylibstored_src
	MANIFEST.in
	LICENSE
	README.md
	libstored/__init__.py
	libstored/cli/__init__.py
	libstored/cli/__main__.py
	libstored/gui/__init__.py
	libstored/gui/__main__.py
	${CMAKE_CURRENT_SOURCE_DIR}/libstored/gui/gui_qrc.py
	libstored/csv.py
	libstored/protocol.py
	libstored/serial2zmq.py
	libstored/stdio2zmq.py
	libstored/stream2zmq.py
	libstored/wrapper/__init__.py
	libstored/wrapper/serial/__init__.py
	libstored/wrapper/serial/__main__.py
	libstored/wrapper/stdio/__init__.py
	libstored/wrapper/stdio/__main__.py
	libstored/zmq_client.py
	libstored/zmq_server.py
	libstored/visu/__init__.py
	libstored/visu/__main__.py
	setup.py
)

if(LIBSTORED_EXAMPLES)
	list(APPEND pylibstored_src
		libstored/visu/example/example.qrc
		libstored/visu/example/main.qml
	)
endif()

execute_process(
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/setup.py -V
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	OUTPUT_VARIABLE pylibstored_version
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_subdirectory(libstored/visu)
add_subdirectory(libstored/gui)

set(pylibstored_wheel_file "libstored-${pylibstored_version}-py3-none-any.whl" CACHE INTERNAL "")
set(pylibstored_wheel "${CMAKE_CURRENT_BINARY_DIR}/dist/${pylibstored_wheel_file}" CACHE INTERNAL "")

set(_wheel_deps ${pylibstored_src} gui-qrc)

if(LIBSTORED_EXAMPLES)
	set(_wheel_deps ${_wheel_deps} visu-example)
endif()

add_custom_command(
	OUTPUT ${pylibstored_wheel}
	DEPENDS ${_wheel_deps}
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/setup.py -q
		sdist -d ${CMAKE_CURRENT_BINARY_DIR}/dist
		bdist_wheel -b ${CMAKE_CURRENT_BINARY_DIR}/build -d ${CMAKE_CURRENT_BINARY_DIR}/dist
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Generate pylibstored distributables"
	VERBATIM
)

add_custom_target(pylibstored-dist ALL
	DEPENDS ${pylibstored_wheel}
)

if(LIBSTORED_PIP_INSTALL_USER)
	add_custom_target(pylibstored-install
		DEPENDS ${pylibstored_wheel}
		COMMAND ${PYTHON_EXECUTABLE} -m pip install --user --upgrade --no-deps --force-reinstall ${pylibstored_wheel}
	)

else()
	add_custom_target(pylibstored-install
		DEPENDS ${pylibstored_wheel}
		COMMAND ${PYTHON_EXECUTABLE} -m pip install --upgrade --no-deps --force-reinstall ${pylibstored_wheel}
	)
endif()

# Backwards compatibility
set(ed2_wheel_file ${pylibstored_wheel_file})
set(ed2_wheel ${pylibstored_wheel_file})
add_custom_target(ed2-dist DEPENDS pylistored-dist)
add_custom_target(ed2-install DEPENDS pylistored-install)

install(CODE "
	execute_process(
		COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target pylibstored-install
		RESULT_VARIABLE _res
	)
	if(NOT \${_res} EQUAL 0)
		message(FATAL_ERROR \"pylibstored-install failed\")
	endif()
")

