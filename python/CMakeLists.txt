# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set(pylibstored_src
    LICENSE
    README.md
    README.md.license
    libstored/__init__.py
    libstored/asyncio/__init__.py
    libstored/asyncio/csv.py
    libstored/asyncio/event.py
    libstored/asyncio/tk.py
    libstored/asyncio/worker.py
    libstored/asyncio/zmq.py
    libstored/cli/__init__.py
    libstored/cli/__main__.py
    libstored/cmake/__init__.py
    libstored/cmake/__main__.py
    libstored/exceptions.py
    libstored/generator/__init__.py
    libstored/generator/__main__.py
    libstored/generator/dsl/__init__.py
    libstored/generator/dsl/grammar.tx
    libstored/generator/dsl/types.py
    libstored/gui/__init__.py
    libstored/gui/__main__.py
    libstored/gui/twotone_bug_report_black_48dp.png
    libstored/gui/twotone_bug_report_black_48dp.png.license
    libstored/heatshrink.py
    libstored/log/__init__.py
    libstored/log/__main__.py
    libstored/protocol/__init__.py
    libstored/protocol/protocol.py
    libstored/protocol/serial.py
    libstored/protocol/stdio.py
    libstored/protocol/util.py
    libstored/protocol/zmq.py
    libstored/tk.py
    libstored/version.py
    libstored/wrapper/__init__.py
    libstored/wrapper/serial/__init__.py
    libstored/wrapper/serial/__main__.py
    libstored/wrapper/stdio/__init__.py
    libstored/wrapper/stdio/__main__.py
)

set(package_data_
    cmake/FindLibstored.cmake.tmpl
    cmake/LibstoredStoresConfig.cmake.in
    cmake/libstored.cmake
    dist/common/FindHeatshrink.cmake
    dist/common/FindZeroMQ.cmake
    dist/common/FindZth.cmake
    doc/libstored-src.spdx.tmpl
    doc/SHA1SUM.tmpl
    doc/store.csv.tmpl
    doc/store.py.tmpl
    doc/store.rtf.tmpl
    doc/store.yml.tmpl
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
    version/version.txt.license
    .clang-tidy
    CMakeLists.txt.tmpl
    LICENSES/CC0-1.0.txt
    LICENSES/MIT.txt
    LICENSES/MPL-2.0.txt
)

set(package_data "")
foreach(F IN LISTS package_data_)
	configure_file(
		${PROJECT_SOURCE_DIR}/${F} ${CMAKE_CURRENT_SOURCE_DIR}/libstored/data/${F} COPYONLY
	)
	list(APPEND package_data ${CMAKE_CURRENT_SOURCE_DIR}/libstored/data/${F})
endforeach()

if(LIBSTORED_DEV)
	add_custom_target(
		pylibstored-reuse ALL
		COMMAND ${PYTHON_EXECUTABLE} -m reuse --root . lint
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libstored/data
		COMMENT "Checking pylibstored generator REUSE compliance"
		VERBATIM
	)
endif()

file(
	GENERATE
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/libstored/data/ignore
	CONTENT ""
)

add_launch_json_pymod(libstored.gui libstored.gui -v -v)
add_launch_json_pymod(libstored.log libstored.log -v -v /rand)
add_launch_json_pymod(libstored.cli libstored.cli -v -v)
if(TARGET 7_protocol OR LIBSTORED_EXAMPLES)
	add_launch_json_pymod(
		libstored.wrapper.stdio
		libstored.wrapper.stdio
		-v
		-v
		-S
		segment,arq,crc16,ascii,pubterm,stdin
		$<TARGET_FILE:7_protocol>
	)
endif()

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

set(_wheel_deps ${pylibstored_src} ${package_data} MANIFEST.in pyproject.toml setup.cfg version.txt)

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

	get_filename_component(pylibstored_wheel_file ${pylibstored_wheel} NAME)
	get_filename_component(pylibstored_tar_file ${pylibstored_tar} NAME)

	install(
		CODE "
		execute_process(
			COMMAND \"${CMAKE_COMMAND}\" -E make_directory \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/libstored\"
			COMMAND \"${CMAKE_COMMAND}\" -E copy_if_different \"${pylibstored_wheel}\"
				\"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/libstored/${pylibstored_wheel_file}\"
			COMMAND \"${CMAKE_COMMAND}\" -E copy_if_different \"${pylibstored_tar}\"
				\"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/libstored/${pylibstored_tar_file}\"
			COMMAND \"${CMAKE_COMMAND}\" --build \"${CMAKE_BINARY_DIR}\" --target pylibstored-install
			RESULT_VARIABLE _res
		)
		if(NOT \${_res} EQUAL 0)
			message(FATAL_ERROR \"pylibstored-install failed\")
		endif()
	"
	)
endif()
