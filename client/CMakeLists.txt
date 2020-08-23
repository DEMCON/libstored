list(APPEND ed2_src
	LICENSE
	README.md
	ed2/__init__.py
	ed2/cli/__init__.py
	ed2/cli/__main__.py
	ed2/gui/__init__.py
	ed2/gui/__main__.py
	ed2/gui/gui_client.qml
	ed2/gui/twotone_bug_report_black_48dp.png
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
	setup.py
)

execute_process(
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/setup.py -V
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	OUTPUT_VARIABLE ed2_version
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/ed2-dist.timestamp
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dist/ed2-${ed2_version}-py3-none-any.whl
	DEPENDS ${ed2_src}
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/setup.py -q sdist bdist_wheel
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_SOURCE_DIR}/ed2-dist.timestamp
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Generate ed2 distributables"
	VERBATIM
)

add_custom_target(ed2-dist ALL
	DEPENDS ed2-dist.timestamp
)

add_custom_target(ed2-install
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ed2-dist.timestamp
	COMMAND ${PYTHON_EXECUTABLE} -m pip install --upgrade --no-deps --force-reinstall ${CMAKE_CURRENT_SOURCE_DIR}/dist/ed2-${ed2_version}-py3-none-any.whl
)

