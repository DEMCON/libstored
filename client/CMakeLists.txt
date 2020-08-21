add_custom_command(
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/ed2-dist.timestamp
	DEPENDS setup.py
	DEPENDS README.md
	DEPENDS ed2/__init__.py
	DEPENDS ed2/serial2zmq.py
	DEPENDS ed2/stdio2zmq.py
	DEPENDS ed2/stream2zmq.py
	DEPENDS ed2/zmq_client.py
	DEPENDS ed2/zmq_server.py
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/setup.py sdist bdist_wheel
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
	COMMAND ${PYTHON_EXECUTABLE} -m pip install ${CMAKE_CURRENT_SOURCE_DIR}/dist/ed2-*.whl
)

