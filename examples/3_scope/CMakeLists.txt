add_executable(3_scope main.cpp)
libstored_generate(TARGET 3_scope STORES ExampleScope.st)
install(TARGETS 3_scope RUNTIME DESTINATION bin)

