find_package(Doxygen)

if(NOT DOXYGEN_FOUND)
	message(FATAL_ERROR "Doxygen is needed to build the documentation.")
endif()

find_program(SPHINX_EXECUTABLE sphinx-build REQUIRED)

configure_file(Doxyfile.in Doxyfile)

file(GLOB_RECURSE DOXY_SRC LIST_DIRECTORIES false CONFIGURE_DEPENDS
	${PROJECT_SOURCE_DIR}/examples/*/*.h
	${PROJECT_SOURCE_DIR}/examples/*/*.cpp
	${PROJECT_SOURCE_DIR}/include/*.h
	${PROJECT_SOURCE_DIR}/include/stored
	${PROJECT_SOURCE_DIR}/include/libstored/*.h
	${PROJECT_SOURCE_DIR}/src/*.cpp
	${PROJECT_BINARY_DIR}/examples/meta/*.h
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/.timestamp_doxy
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/example_filter.py
	DEPENDS ${DOXY_SRC}
	DEPENDS all-libstored-generate
	COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/.timestamp_doxy
	COMMENT "Parse sources with doxygen"
	VERBATIM
)

add_custom_target(doc-doxygen
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/.timestamp_doxy
)

# For README.rst, link the examples directory for screenshots
if(WIN32)
	execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/doc/examples/zmqserver")
	execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/examples/zmqserver/zmqserver_screenshot.png" "${CMAKE_CURRENT_SOURCE_DIR}/doc/examples/zmqserver/zmqserver_screenshot.png")
elseif(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/doc/examples)
	execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${PROJECT_SOURCE_DIR}/examples" "${CMAKE_CURRENT_SOURCE_DIR}/doc/examples")
endif()

if(NOT MINGW)
	set(SPHINX_CONDITIONAL -t threads)
endif()

file(GLOB_RECURSE SPHINX_SRC LIST_DIRECTORIES false CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/doc/*.rst
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/.timestamp_html
	DEPENDS all-libstored-generate
	DEPENDS ${SPHINX_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/conf.py
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/.timestamp_doxy
	COMMAND ${CMAKE_COMMAND} -E env DOXYGEN_BUILD=${CMAKE_CURRENT_BINARY_DIR}/doxygen
		${SPHINX_EXECUTABLE}
		-M html
		$<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}>
		$<SHELL_PATH:${CMAKE_CURRENT_BINARY_DIR}/_build>
		-c $<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}>
		-d $<SHELL_PATH:${CMAKE_CURRENT_BINARY_DIR}/_build/doctree-html>
		-q
		${SPHINX_CONDITIONAL}
	COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/presentation/libstored.sozi.html ${CMAKE_CURRENT_BINARY_DIR}/_build/html
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/.timestamp_html
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	VERBATIM
	COMMENT "Generating API documentation (see ${CMAKE_CURRENT_BINARY_DIR}/_build/html/index.html)"
)

add_custom_target(doc ALL
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/.timestamp_html
)
