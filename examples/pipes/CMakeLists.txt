set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)

add_executable(pipes main.cpp)
libstored_generate(TARGET pipes STORES ExamplePipes.st)
install(TARGETS pipes RUNTIME DESTINATION bin)
