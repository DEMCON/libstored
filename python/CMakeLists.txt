# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2023  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

set(pylibstored_src
    LICENSE
    README.md
    libstored/__init__.py
    libstored/cli/__init__.py
    libstored/cli/__main__.py
    libstored/cmake/__init__.py
    libstored/cmake/__main__.py
    libstored/generator/__init__.py
    libstored/generator/__main__.py
    libstored/generator/dsl/types.py
    libstored/generator/dsl/grammar.tx
    libstored/gui/__init__.py
    libstored/gui/__main__.py
    libstored/log/__init__.py
    libstored/log/__main__.py
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
)

set(package_data_
    cmake/FindLibstored.cmake.tmpl
    cmake/LibstoredStoresConfig.cmake.in
    cmake/libstored.cmake
    dist/common/FindHeatshrink.cmake
    dist/common/FindZeroMQ.cmake
    dist/common/FindZth.cmake
    doc/store.csv.tmpl
    doc/store.py.tmpl
    doc/store.rtf.tmpl
    fpga/doc/vhdl.rst
    fpga/rtl/pkg.vhd
    fpga/rtl/protocol.vhd
    fpga/rtl/store.vhd.tmpl
    fpga/rtl/store_pkg.vhd.tmpl
    fpga/rtl/util.vhd
    fpga/rtl/variable.vhd
    fpga/tb/tb_pkg.vhd
    fpga/vivado/vivado.tcl.tmpl
    include/libstored/allocator.h
    include/libstored/components.h
    include/libstored/compress.h
    include/libstored/config.h
    include/libstored/debugger.h
    include/libstored/directory.h
    include/libstored/fifo.h
    include/libstored/macros.h
    include/libstored/pipes.h
    include/libstored/poller.h
    include/libstored/protocol.h
    include/libstored/signal.h
    include/libstored/spm.h
    include/libstored/synchronizer.h
    include/libstored/types.h
    include/libstored/util.h
    include/libstored/version.h
    include/libstored/zmq.h
    include/libstored/store.h.tmpl
    include/stored.h
    include/stored_config.h
    include/stored
    src/compress.cpp
    src/debugger.cpp
    src/directory.cpp
    src/pipes.cpp
    src/poller.cpp
    src/protocol.cpp
    src/synchronizer.cpp
    src/util.cpp
    src/zmq.cpp
    src/store.cpp.tmpl
    version/CMakeLists.txt
    version/version.txt
    .clang-tidy
    CMakeLists.txt.tmpl
)

set(package_data "")
foreach(F IN LISTS package_data_)
	configure_file(
		${PROJECT_SOURCE_DIR}/${F} ${CMAKE_CURRENT_SOURCE_DIR}/libstored/data/${F} COPYONLY
	)
	list(APPEND package_data ${CMAKE_CURRENT_SOURCE_DIR}/libstored/data/${F})
endforeach()

file(
	GENERATE
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/libstored/data/ignore
	CONTENT ""
)

if(LIBSTORED_EXAMPLES)
	list(APPEND pylibstored_src libstored/visu/example/example.qrc
	     libstored/visu/example/main.qml
	)

	libstored_visu(
		visu-example ${CMAKE_CURRENT_SOURCE_DIR}/libstored/visu/visu_example.py
		libstored/visu/example/main.qml
	)
endif()

add_subdirectory(libstored/gui)

macro(find_latest file pattern)
	file(GLOB ${file}_ALL CONFIGURE_DEPENDS ${pattern})

	if(NOT "${${file}_ALL}" STREQUAL "")
		# This is an alphabetical sort, not really based on the version.  This could lead to
		# errors if you rebuild everything while the version changes.
		list(SORT ${file}_ALL)
		list(REVERSE ${file}_ALL)
		list(GET ${file}_ALL 0 ${file})
	else()
		set(${file})
	endif()
endmacro()

find_latest(pylibstored_wheel ${CMAKE_CURRENT_BINARY_DIR}/libstored*.whl)
find_latest(pylibstored_tar ${CMAKE_CURRENT_BINARY_DIR}/libstored*.tar.gz)

set(_wheel_deps
    ${pylibstored_src}
    ${package_data}
    gui-qrc
    MANIFEST.in
    pyproject.toml
    setup.cfg
    version.txt
)

if(LIBSTORED_EXAMPLES)
	set(_wheel_deps ${_wheel_deps} visu-example)
endif()

if("${pylibstored_wheel_ALL}${pylibstored_tar_ALL}" STREQUAL "")
	set(cmake_rm echo "No old whl files found")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "3.17")
	set(cmake_rm rm -f)
else()
	set(cmake_rm remove -f)
endif()

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/.timestamp
	# BYPRODUCTS ${pylibstored_wheel} ${pylibstored_tar}
	DEPENDS ${_wheel_deps}
	COMMAND ${CMAKE_COMMAND} -E ${cmake_rm} ${pylibstored_wheel_ALL} ${pylibstored_tar_ALL}
	COMMAND ${PYTHON_EXECUTABLE} -m build -o ${CMAKE_CURRENT_BINARY_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/.timestamp
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "Generate pylibstored distributables"
	VERBATIM
)

add_custom_target(pylibstored-dist ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/.timestamp)

if(NOT "${pylibstored_wheel}" STREQUAL "")
	if(LIBSTORED_PIP_INSTALL_USER)
		add_custom_target(
			pylibstored-install
			DEPENDS pylibstored-dist
			COMMAND ${PYTHON_EXECUTABLE} -m pip install --user --upgrade --no-deps
				--force-reinstall ${pylibstored_wheel}
		)

	else()
		add_custom_target(
			pylibstored-install
			DEPENDS pylibstored-dist
			COMMAND ${PYTHON_EXECUTABLE} -m pip install --upgrade --no-deps
				--force-reinstall ${pylibstored_wheel}
		)
	endif()
endif()

install(
	CODE "
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/libstored
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${pylibstored_wheel} ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/libstored
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${pylibstored_tar} ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/libstored
		COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target pylibstored-install
		RESULT_VARIABLE _res
	)
	if(NOT \${_res} EQUAL 0)
		message(FATAL_ERROR \"pylibstored-install failed\")
	endif()
"
)
