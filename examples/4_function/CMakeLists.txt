add_executable(4_function main.cpp)
libstored_generate(TARGET 4_function STORES ExampleFunction.st)
install(TARGETS 4_function RUNTIME DESTINATION bin)

