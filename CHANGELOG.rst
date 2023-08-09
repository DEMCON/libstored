CHANGELOG
=========

All notable changes to this project will be documented in this file.

The format is based on `Keep a Changelog`_, and this project adheres to `Semantic Versioning`_.

.. _Keep a Changelog: https://keepachangelog.com/en/1.0.0/
.. _Semantic Versioning: https://semver.org/spec/v2.0.0.html



`Unreleased`_
-------------

Changed
```````

- ASan checks on the correct usage of entry/exit RO/X of stores.

Fixed
`````

- Fix encode queue size computation of stored::ArqLayer.

.. _Unreleased: https://github.com/DEMCON/libstored/compare/v1.5.0...HEAD



`1.5.0`_ - 2023-07-07
---------------------

Added
`````

- Python `libstored.log` command line logging tool to easily generate a CSV
  file with samples.

Changed
```````

- Get a compile error when a store class does provide the implementation of all
  functions, and provide a store wrapper to add default function
  implementations.
- Check against recursive entry/exit RO/X locks per store.
- Replaced deprecated ``setup.py`` and removed deprecated ``ed2`` package.
- Improved support to ``include(libstored)`` in your CMake project.

.. _1.5.0: https://github.com/DEMCON/libstored/releases/tag/v1.5.0



`1.4.0`_ - 2023-04-06
---------------------

Added
`````

- First-order high-pass filter.
- Pipes, to process data using functional composition.
- A Signal class (with Signalling store wrapper) to call functions when the
  signal is triggered (or a store variable is changed).

Fixed
`````

- Properly calling hooks by the Synchronizer.
- Minor issues with pylibstored.

.. _1.4.0: https://github.com/DEMCON/libstored/releases/tag/v1.4.0



`1.3.1`_ - 2022-10-21
---------------------

Fixed
`````

- PySide6 6.4.0 support in changed ``enum`` handling. However, the same issue
  exists in matplotlib (issue `#24155`_).  To get plotting working again,
  matplotlib>=3.6.2 or PySide6<6.4.0 is required.

.. _#24155: https://github.com/matplotlib/matplotlib/issues/24155
.. _1.3.1: https://github.com/DEMCON/libstored/releases/tag/v1.3.1



`1.3.0`_ - 2022-10-20
---------------------

Added
`````

- Maximum error for PID.
- Stream visualization in the Embedded Debugger.

Changed
```````

- Switch license to MPLv2.

Fixed
`````

- Fix in computing ``stored::Ramp`` acceleration and speed.
- Fix in compressed Debugger streams upon internal buffer overflow.
- Handle unaligned memory access properly in store objects.

.. _1.3.0: https://github.com/DEMCON/libstored/releases/tag/v1.3.0



`1.2.0`_ - 2022-03-28
---------------------

Added
`````

- Add QObject/QML wrapper for stores to access a store directly in QML.
- Generate store variable changed callbacks.
- Allow string variables to be initialized.
- Generate store meta data in ``doc/<store>Meta.py``

Changed
```````

- Replaced ``UNUSED_PAR`` by a more portable ``UNUSED`` macro.

Fixed
`````

- Improved QtCreator integration, such as store code-completion
- Fix support for disabling RTTI
- Fix support for disabling exceptions

.. _1.2.0: https://github.com/DEMCON/libstored/releases/tag/v1.2.0



`1.1.0`_ - 2022-01-07
---------------------

Added
`````

- Add support for ninja, and default to it in build scripts.
- Use ``matplotlib`` for plotting signals by ``libstored.gui``.

Changed
```````

- Migrate pylibstored from PySide2 to PySide6.
- Restructure ``scripts`` directory into ``dist``.
- Replace poller API.
- ``libstored_lib`` and ``libstored_generate`` now take keyword-based
  parameters, but old (positional) interface is still supported.
- Improve Zth_ integration for fiber-aware polling.

Removed
```````

- Drop Ubuntu 18.04 support. PySide6 requires Ubuntu 20.04 or later.

.. _1.1.0: https://github.com/DEMCON/libstored/releases/tag/v1.1.0
.. _Zth: https://github.com/jhrutgers/zth



`1.0.0`_ - 2021-08-25
---------------------

Initial version.

Added
`````

- Store generator for C++ and VHDL
- C++ library to access the store in your application
- VHDL entities to setup synchronization between VHDL and a C++ store
- python library with the Embedded Debugger protocol to connect to an
  application
- A presentation
- Examples
- (Unit) tests
- Some documentation

.. _1.0.0: https://github.com/DEMCON/libstored/releases/tag/v1.0.0
