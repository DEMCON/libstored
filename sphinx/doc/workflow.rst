Workflow
========

To use libstored, this is basically what you have to do:

.. uml::

   start
   :store.st file(s)/
   :Generate store
   (by libstored's CMakeLists.txt);
   fork
      :C++ static library/
      :Link to common libstored library;
      :Integrate store in project;
      :Instantiate Debugger instance,
      and build protocol stack;
      :Instantiate Synchronizer instance,
      and build protocol stack;
   fork again
      :VHDL package and entity/
      :Include common libstored
      package/entities;
      :Integrate store in project;
      :Build store's synchronizer
      protocol stack;
   fork again
      :Documentation/
      :Integrate in project's docs;
   end fork
   :Integrate on device;
   :Debug with Python client;
   stop

The resulting architecture of your application could look like this:

.. uml::

   node "Microcontroller" {
      rectangle storeA1 [
      Store A (C++)
      ]

      rectangle storeB1 [
      Store B (C++)
      ]

      cloud app1 [
         Application
      ]
      storeA1 -- app1
      storeB1 -- app1

      rectangle debugger [
      Debugger
      ]
      storeA1 -- debugger
      storeB1 -- debugger

      rectangle debugstack [
      DebugArqLayer
      ----
      Crc16Layer
      ----
      AsciiEscapeLayer
      ----
      TerminalLayer
      ----
      StdioLayer
      ]
      debugger -- debugstack

      note right of debugstack
         Any stack as required
         for you infrastructure.
      end note

      rectangle sync1 [
      Synchronizer
      ]

      storeA1 -- sync1

      rectangle syncstack [
      AsciiEscapeLayer
      ----
      TerminalLayer
      ----
      FileLayer
      ]
      sync1 -- syncstack
   }

   () "lossy UART" as UART
   debugstack -- UART

   () "lossless UART" as UARTsync
   syncstack -- UARTsync

   node "PC" {
      rectangle wrapper [
         ed2.ZmqServer
         ----
         DebugArqLayer
         ----
         Crc16Layer
         ----
         AsciiEscapeLayer
         ----
         TerminalLayer
         ----
         ed2.wrapper.serial
      ]
      wrapper -- UART

      note right of wrapper
         The configured stack must
         match the one at the
         embedded side.
      end note

      () ZMQ
      wrapper -- ZMQ

      rectangle gui [
         ed2.gui
      ]
      ZMQ -- gui

      rectangle visu [
         ed2.visu
      ]
      ZMQ -- visu
   }

   node "FPGA" {
      rectangle storeA2 [
      Store A (VHDL)
      includes Synchronizer
      ]

      rectangle fpgastack [
      AsciiEscapeLayer
      ----
      TerminalLayer
      ----
      UARTLayer
      ]
      storeA2 -- fpgastack

      cloud app2 [
         Application
      ]
      storeA2 -- app2
   }

   UARTsync -- fpgastack




