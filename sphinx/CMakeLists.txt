find_package(Doxygen)

if(NOT DOXYGEN_FOUND)
	message(FATAL_ERROR "Doxygen is needed to build the documentation.")
endif()

find_program(MAKE_EXECUTABLE make REQUIRED)
find_program(SPHINX_EXECUTABLE sphinx-build REQUIRED)

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

add_custom_target(
	doc ALL
	COMMAND $<TARGET_FILE:Doxygen::doxygen>
	COMMAND ${SPHINX_EXECUTABLE} -M html $<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}> $<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}/_build> -c $<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}> ${SPHINX_CONDITIONAL}
	COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/presentation/libstored.sozi.html ${CMAKE_CURRENT_SOURCE_DIR}/_build/html
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Generating API documentation (see sphinx/_build/html/sphinx/index.html)"
	VERBATIM
)

