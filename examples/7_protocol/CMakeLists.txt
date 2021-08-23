add_executable(7_protocol main.cpp)
libstored_generate(7_protocol ExampleProtocol.st)
install(TARGETS 7_protocol RUNTIME)

