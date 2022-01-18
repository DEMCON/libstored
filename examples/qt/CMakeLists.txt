add_executable(qt main.cpp)
libstored_generate(TARGET qt STORES ExampleQtStore.st QT)
install(TARGETS qt RUNTIME DESTINATION bin)
