add_executable(concurrency main.cpp)
libstored_generate(concurrency ExampleConcurrencyMain.st ExampleConcurrencyControl.st)

set_target_properties(concurrency
	PROPERTIES
		CXX_STANDARD 14
		CXX_STANDARD_REQUIRED YES
)
