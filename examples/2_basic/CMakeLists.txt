add_executable(2_basic main.cpp)
libstored_generate(TARGET 2_basic STORES ExampleBasic.st)
install(TARGETS 2_basic RUNTIME DESTINATION bin)

