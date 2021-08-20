add_executable(concurrency main.cpp)
set_target_properties(concurrency
	PROPERTIES
		CXX_STANDARD 14
		CXX_STANDARD_REQUIRED YES
)

libstored_generate(concurrency ExampleConcurrencyMain.st ExampleConcurrencyControl.st)

find_package(Threads)
target_link_libraries(concurrency PUBLIC ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS concurrency RUNTIME)

