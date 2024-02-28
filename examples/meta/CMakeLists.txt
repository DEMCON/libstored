# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

add_executable(meta main.cpp ${CMAKE_CURRENT_BINARY_DIR}/LoggingExampleMeta.h)
libstored_generate(TARGET meta STORES ExampleMeta.st)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/LoggingExampleMeta.h
	DEPENDS meta-libstored-generate
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libstored/doc/ExampleMetaMeta.py
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/generator.py
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/LoggingWrapper.h.tmpl
	COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generator.py"
		-m "${CMAKE_CURRENT_SOURCE_DIR}/libstored/doc/ExampleMetaMeta.py"
		-t "${CMAKE_CURRENT_SOURCE_DIR}/LoggingWrapper.h.tmpl"
		-o "${CMAKE_CURRENT_BINARY_DIR}/LoggingExampleMeta.h"
	VERBATIM
	COMMENT "Generate logging wrapper for ExampleMeta"
)

add_custom_target(meta-generate
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/LoggingExampleMeta.h
)

target_include_directories(meta PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

install(TARGETS meta RUNTIME DESTINATION bin)
