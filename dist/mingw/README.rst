

..
   SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
   
   SPDX-License-Identifier: CC0-1.0

libstored for Windows using MinGW on Ubuntu
===========================================

This directory contains files to build libstored for Windows, cross-compiling
from Ubuntu using MinGW.  This builds the examples and tests for demonstration
and verification purposes.

1. Run ``bootstrap.sh`` once to install all required dependencies.
2. Run ``build.sh`` to build and optionally test libstored.  For a list of
   flags to pass to ``build.sh``, run ``build.sh -h`` for help.

Additionally,

- ``test_config.sh`` tests various combinations of flags of ``build.sh``.
- ``toolchain-mingw.cmake`` defines the toolchain for cross-compiling.
