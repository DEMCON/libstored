add_executable(3_scope main.cpp)
libstored_generate(3_scope ExampleScope.st)
install(TARGETS 3_scope RUNTIME DESTINATION bin)

