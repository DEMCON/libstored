# Assume this is your project, and you have generated the FindLibstored.cmake file, which can be
# found in the CMAKE_MODULE_PATH.

cmake_minimum_required(VERSION 3.5)
project(1_hello_findpackage)

# If you want to have ZMQ, heatshrink, Zth, and Qt, you have to do a find_package() yourself, and
# set LIBSTORED_HAVE_LIBZMQ (and friends) to ON. Refer to the list of options() at the top of of
# cmake/libstored.cmake for details.

find_package(Libstored REQUIRED)

# Now, just continue with defining your application.
set(HELLO_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../1_hello)
add_executable(1_hello_findpackage ${HELLO_PATH}/main.cpp)

libstored_generate(
	TARGET 1_hello_findpackage STORES ${HELLO_PATH}/ExampleHello.st DESTINATION
	${CMAKE_CURRENT_BINARY_DIR}
)

install(TARGETS 1_hello_findpackage RUNTIME)
