add_compile_options(-Wall -Wextra -Werror -Wdouble-promotion -Wformat=2 -Wundef -Wconversion -ffunction-sections -fdata-sections)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
endif()

add_subdirectory(1_hello)
add_subdirectory(2_basic)
add_subdirectory(3_scope)
add_subdirectory(4_function)
add_subdirectory(5_debug)

