if(MSVC)
	add_compile_options(/W1 /WX)

	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		add_compile_options(/MTd)
	else()
		add_compile_options(/MT)
	endif()
else()
	add_compile_options(-Wall -Wextra -Werror -Wdouble-promotion -Wformat=2 -Wconversion -ffunction-sections -fdata-sections)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${PROJECT_SOURCE_DIR}/extern/googletest" "extern/googletest")

add_custom_target(teststore)
libstored_generate(teststore TestStore.st)

macro(libstored_add_test TESTNAME)
	add_executable(${TESTNAME} ${ARGN})
	target_link_libraries(${TESTNAME} gtest gmock gtest_main teststore-libstored)
	gtest_discover_tests(${TESTNAME}
		WORKING_DIRECTORY ${PROJECT_DIR}
		PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_DIR}"
	)
	set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()

libstored_add_test(test_types test_types.cpp)
libstored_add_test(test_init test_init.cpp)
libstored_add_test(test_function test_function.cpp)
libstored_add_test(test_array test_array.cpp)
libstored_add_test(test_directory test_directory.cpp)
libstored_add_test(test_spm test_spm.cpp)

if(HAVE_VALGRIND AND LIBSTORED_EXAMPLES)
	macro(libstored_example_test EXAMPLE)
		add_test(NAME ${EXAMPLE} COMMAND valgrind --error-exitcode=1 --leak-check=full ${CMAKE_CURRENT_BINARY_DIR}/../examples/${EXAMPLE}/${EXAMPLE})
	endmacro()

	libstored_example_test(1_hello)
	libstored_example_test(2_basic)
	libstored_example_test(3_scope)
	libstored_example_test(4_function)
	libstored_example_test(5_debug)
	libstored_example_test(6_sync)
endif()

