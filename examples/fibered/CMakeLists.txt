add_executable(fibered main.cpp)
libstored_generate(TARGET fibered STORES ExampleFibered.st ZTH)
install(TARGETS fibered RUNTIME DESTINATION bin)

