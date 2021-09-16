add_executable(4_function main.cpp)
libstored_generate(4_function ExampleFunction.st)
install(TARGETS 4_function RUNTIME DESTINATION bin)

