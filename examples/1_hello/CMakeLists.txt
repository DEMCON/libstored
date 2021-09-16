add_executable(1_hello main.cpp)
libstored_generate(1_hello ExampleHello.st)
install(TARGETS 1_hello RUNTIME DESTINATION bin)

