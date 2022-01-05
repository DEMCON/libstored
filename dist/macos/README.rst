libstored for macOS
===================

This directory contains files to build libstored for macOS (tested with 10.15).
This builds the examples and tests for demonstration and verification purposes.

1. Run ``bootstrap.sh`` once to install all required dependencies.
2. Run ``build.sh`` to build and optionally test libstored.  For a list of
   flags to pass to ``build.sh``, run ``build.sh -h`` for help.

Additionally,

- ``test_config.sh`` tests various combinations of flags of ``build.sh``.
