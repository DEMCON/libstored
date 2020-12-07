find_package(Doxygen)

if(NOT DOXYGEN_FOUND)
	message(FATAL_ERROR "Doxygen is needed to build the documentation.")
endif()

find_program(MAKE_EXECUTABLE make REQUIRED)
find_program(SPHINX_EXECUTABLE sphinx-build REQUIRED)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/doc/examples)
	# For README.md, link the examples directory for screenshots
	execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${PROJECT_SOURCE_DIR}/examples" "${CMAKE_CURRENT_SOURCE_DIR}/doc/examples")
endif()

add_custom_target(
	doc ALL
	COMMAND $<TARGET_FILE:Doxygen::doxygen>
	COMMAND ${SPHINX_EXECUTABLE} -M html $<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}> $<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}/_build> -c $<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}>
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Generating API documentation (see sphinx/_build/html/sphinx/index.html)"
	VERBATIM
)

