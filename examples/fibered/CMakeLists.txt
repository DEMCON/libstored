add_executable(fibered main.cpp)
libstored_generate(TARGET fibered STORES ExampleFibered.st ZTH ZMQ)
install(TARGETS fibered RUNTIME DESTINATION bin)

