# (py)libstored -- Embedded Debugger protocol implementation for libstored

This package contains the client implementation of the Embedded Debugger
protocol (version 2), and a few helper classes and modules to debug your
application. See [libstored](https://github.com/DEMCON/libstored) how to create
an application that is to be debugged using these clients.

The synchronization protocol of libstored is not available in python (yet?).

See the [documentation](https://demcon.github.io/libstored/doc/py.html).

## Interesting modules

Execute these modules like `python3 -m libstored.gui`, optionally with `-h`
argument to get some more help:

- `libstored.gui`: a GUI that connects to a debug target.  The GUI has by
  default high DPI support. If the scaling is not satisfactory, try setting the
  `QT_SCALE_FACTOR` environment variable before starting the GUI, or use
  Ctrl+Scroll wheel to dynamically resize the fonts.
- `libstored.cli`: a command line interface that connects to a debug target.
- `libstored.wrapper.stdio`: a stdin/stdout wrapper, which is a bridge between
  Embedded Debugger messages within the stdin/stdout streams of the application
  to a ZeroMQ socket interface, which in turn can be used to connect
  `libstored.gui` or `libstored.cli` to.
- `libstored.wrapper.serial`: like `libstored.wrapper.stdio`, but using
  `pyserial` instead of stdin/stdout.

## Interesting classes

The following classes are particularly interesting:

- `libstored.ZmqClient`
- `libstored.ZmqServer`
- `libstored.Stdio2Zmq`
- `libstored.Serial2Zmq`

