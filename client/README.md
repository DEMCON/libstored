# ed2 -- Embedded Debugger protocol implementation for libstored

This package contains the client implementation of the Embedded Debugger
protocol (version 2), and a few helper classes and modules to debug your
application. See [libstored](https://github.com/DEMCON/libstored) how to create
an application that is to be debugged using these clients.

## Interesting modules

Execute these modules like `python3 -m ed2.gui`, optionally with `-?` argument
to get some more help:

- `ed2.gui`: a GUI that connects to a debug target.
  The GUI has by default high DPI support. If the scaling is not satisfactory,
  try setting the `QT_SCALE_FACTOR` environment variable before starting the GUI.
- `ed2.cli`: a command line interface that connects to a debug target.
- `ed2.wrapper.stdio`: a stdin/stdout wrapper, which is a bridge between
  Embedded Debugger messages within the stdin/stdout streams of the application
  to a ZeroMQ socket interface, which in turn can be used to connect `ed2.gui`
  or `ed2.cli` to.
- `ed2.wrapper.serial`: like `ed2.wrapper.stdio`, but using `pyserial` instead
  of stdin/stdout.

## Interesting classes

The following classes are particularly interesting:

- `ed2.ZmqClient`
- `ed2.ZmqServer`
- `ed2.Stdio2Zmq`
- `ed2.Serial2Zmq`

