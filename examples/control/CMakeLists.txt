add_executable(control main.cpp)
set_target_properties(control PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
libstored_generate(TARGET control STORES ExampleControl.st)
install(TARGETS control RUNTIME DESTINATION bin)

