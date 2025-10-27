# (py)libstored -- Store generator and Embedded Debugger client

[libstored](https://github.com/DEMCON/libstored) is a generator for a C++ class
(store) with your application's variables, and a tool set to synchronize
updates between processes (including FPGA), and debug it remotely. Refer to the
[documentation](https://demcon.github.io/libstored/doc/py.html) for details and
examples.

This Python package contains:

- The generator itself.
- The client implementation of the Debugger protocol (version 2), and
  a few helper classes and modules to debug your application.

The synchronization protocol of libstored is not available in python (yet?).

## Generator

In short, to generate the store, run `python3 -m libstored.cmake`, which
produces a `FindLibstored.cmake` for you. Then, call `find_package(Libstored)`
in your `CMakeLists.txt`. Afterwards, the CMake function `libstored_generate()`
can be used to create the store (C++ header/source files, VHDL package,
documentation) for you and build it as a static library.

This library includes the Debugger, which provides the server side of the
Debugging client below.

## Debugging client

Execute these modules like `python3 -m libstored.gui`, optionally with `-h`
argument to get some more help:

- `libstored.gui`: a GUI that connects to a debug target.
- `libstored.cli`: a command line interface that connects to a debug target.
- `libstored.wrapper.stdio`: a stdin/stdout wrapper, which is a bridge between
  Embedded Debugger messages within the stdin/stdout streams of the application
  to a ZeroMQ socket interface, which in turn can be used to connect
  `libstored.gui` or `libstored.cli` to.
- `libstored.wrapper.serial`: like `libstored.wrapper.stdio`, but using
  `pyserial` instead of stdin/stdout.
- `libstored.log`: command line tool that connects to a debug target and logs
  samples to CSV.  It is equivalent to passing `-f` to `libstored.gui`, but
  this tool allows easier automation of a specific set of samples.
