add_executable(9_fpga main.cpp)
libstored_generate(9_fpga ExampleFpga.st ExampleFpga2.st)

if(VIVADO_CMD)
	if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vivado/9_fpga/9_fpga.xpr)
		execute_process(
			COMMAND ${VIVADO_CMD} -mode batch -source 9_fpga.tcl
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vivado
		)
	endif()
endif()

