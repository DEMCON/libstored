CHANGELOG
=========

All notable changes to this project will be documented in this file.

The format is based on `Keep a Changelog`_, and this project adheres to `Semantic Versioning`_.

.. _Keep a Changelog: https://keepachangelog.com/en/1.0.0/
.. _Semantic Versioning: https://semver.org/spec/v2.0.0.html



`Unreleased`_
-------------

...

.. _Unreleased: https://github.com/DEMCON/libstored/compare/v1.1.0...HEAD



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
