# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

if(LIBSTORED_HAVE_LIBZMQ)
	add_executable(9_fpga main.cpp)
	libstored_generate(TARGET 9_fpga STORES ExampleFpga.st ExampleFpga2.st)
	install(TARGETS 9_fpga RUNTIME DESTINATION bin OPTIONAL)

	# Not tested with unix pipes.
	if(NOT WIN32)
		set_target_properties(9_fpga PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)
	endif()

	if(VIVADO_CMD)
		if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vivado/9_fpga/9_fpga.xpr)
			execute_process(
				COMMAND ${VIVADO_CMD} -mode batch -source 9_fpga.tcl
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vivado
			)
		endif()
	endif()
endif()
