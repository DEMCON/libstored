# This example takes the installed 1_hello libstored and links to the generated
# static library.  Therefore, this only works after installing the project. So,
# you may have to build again after installing...

# Refer to 1_hello_again/CMakeLists for more information.

if(EXISTS ${CMAKE_INSTALL_PREFIX}/share/cmake/libstored/libstored.cmake)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/1_hello_again.timestamp
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/1_hello_again
		COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_again
			${CMAKE_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/1_hello_again -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
		COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_again
			${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
		COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_again
			${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target install
		COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/1_hello_again.timestamp
	)

	add_custom_target(1_hello_again
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/1_hello_again.timestamp
	)
endif()

