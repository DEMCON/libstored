if(LIBSTORED_HAVE_LIBZMQ)
	add_executable(8_sync main.cpp getopt_mini.cpp)
	libstored_generate(8_sync ExampleSync1.st ExampleSync2.st)
	install(TARGETS 8_sync RUNTIME DESTINATION bin)
endif()

