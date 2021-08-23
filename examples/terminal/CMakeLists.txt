add_executable(terminal main.cpp)
libstored_generate(terminal ExampleTerminal.st)
install(TARGETS terminal RUNTIME)

