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
