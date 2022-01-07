add_executable(6_hooks main.cpp)
libstored_generate(TARGET 6_hooks STORES ExampleHooks.st)
install(TARGETS 6_hooks RUNTIME DESTINATION bin)

