add_executable(6_hooks main.cpp)
libstored_generate(6_hooks ExampleHooks.st)
install(TARGETS 6_hooks RUNTIME)

