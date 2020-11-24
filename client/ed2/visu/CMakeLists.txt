find_program(RCC_EXE pyside2-rcc REQUIRED)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/rcc_example.py
	DEPENDS visu.qrc example/example.qrc example/main.qml
	COMMAND ${RCC_EXE} visu.qrc $<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}/example/example.qrc> -o rcc_example.py
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Generating example rcc"
)

add_custom_target(
	visu-example
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rcc_example.py
)

