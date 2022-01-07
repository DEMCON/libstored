add_executable(terminal main.cpp)
libstored_generate(TARGET terminal STORES ExampleTerminal.st)
install(TARGETS terminal RUNTIME DESTINATION bin)

