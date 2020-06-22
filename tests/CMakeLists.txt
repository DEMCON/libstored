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

