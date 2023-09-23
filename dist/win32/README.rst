

..
   SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
   
   SPDX-License-Identifier: CC0-1.0

libstored for Windows
=====================

This directory contains files to build libstored for Windows.  This builds the
examples and tests for demonstration and verification purposes.

1. Run ``bootstrap.cmd`` once to install all required dependencies. It uses
   Chocolatey_.
2. Run ``build.cmd`` to build and optionally test libstored.  For a list of
   flags to pass to ``build.cmd``, run ``build.cmd -h`` for help.

Additionally,

- ``test_config.cmd`` tests various combinations of flags of ``build.cmd``.
- ``env.cmd`` prepares your environment to find all required tools, such as
  ``cmake`` and ``gcc``. Call this script if you want to build manually,
  without using ``build.cmd``.
- ``venv.cmd`` script to install and activate the Python venv. It is managed
  automatically during the build.  To get access to the Python environment,
  call ``..\venv\Scripts\activate.bat``.

.. _Chocolatey: https://chocolatey.org/
