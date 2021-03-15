set(LIBSTORED_HAVE_HEATSHRINK 0)
add_executable(5_debug main.cpp)
libstored_generate(5_debug ExampleDebugSomeStore.st ExampleDebugAnotherStore.st)

