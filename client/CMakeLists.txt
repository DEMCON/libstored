list(APPEND ed2_src
	MANIFEST.in
	LICENSE
	README.md
	ed2/__init__.py
	ed2/cli/__init__.py
	ed2/cli/__main__.py
	ed2/gui/__init__.py
	ed2/gui/__main__.py
	ed2/gui/gui_client.qml
	ed2/gui/twotone_bug_report_black_48dp.png
	ed2/csv.py
	ed2/protocol.py
	ed2/serial2zmq.py
	ed2/stdio2zmq.py
	ed2/stream2zmq.py
	ed2/wrapper/__init__.py
	ed2/wrapper/serial/__init__.py
	ed2/wrapper/serial/__main__.py
	ed2/wrapper/stdio/__init__.py
	ed2/wrapper/stdio/__main__.py
	ed2/zmq_client.py
	ed2/zmq_server.py
	ed2/visu/__init__.py
	ed2/visu/__main__.py
	ed2/visu/example/example.qrc
	ed2/visu/example/main.qml
	setup.py
)

execute_process(
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/setup.py -V
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	OUTPUT_VARIABLE ed2_version
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_subdirectory(ed2/visu)

set(ed2_wheel_file "ed2-${ed2_version}-py3-none-any.whl" CACHE INTERNAL "")
set(ed2_wheel "${CMAKE_CURRENT_SOURCE_DIR}/dist/${ed2_wheel_file}" CACHE INTERNAL "")

add_custom_command(
	OUTPUT ${ed2_wheel}
	DEPENDS visu-example ${ed2_src}
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/setup.py -q sdist bdist_wheel
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Generate ed2 distributables"
	VERBATIM
)

add_custom_target(ed2-dist ALL
	DEPENDS ${ed2_wheel}
)

add_custom_target(ed2-install
	DEPENDS ${ed2_wheel}
	COMMAND ${PYTHON_EXECUTABLE} -m pip install --upgrade --no-deps --force-reinstall ${ed2_wheel}
)

install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ed2-install)")

