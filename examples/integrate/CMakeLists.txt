# This example shows how to integrate libstored in your own CMake project.  See
# project/CMakeLists.txt that would be your project.

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/project.timestamp
	DEPENDS project/CMakeLists.txt ../1_hello/main.cpp ../1_hello/ExampleHello.st
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/project
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/project
		${CMAKE_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/project -G ${CMAKE_GENERATOR}
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
		-DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/project
		${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/project
		${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target install
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/project.timestamp
	COMMENT "Building 1_hello using included libstored"
)

add_custom_target(1_hello_again_and_again ALL
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/project.timestamp
)
