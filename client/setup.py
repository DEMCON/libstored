#!/usr/bin/env python3
# vim:et

import sys
import os
import setuptools

here = os.path.dirname(os.path.realpath(__file__))

#with open(os.path.join(here, "README.md"), "r") as fh:
#    long_description = fh.read()

setuptools.setup(
	name = 'ed2',
	version = '0.1',
    description = 'Embedded Debugger protocol as used by libstored',
#    long_description=long_description,
#    long_description_content_type="text/markdown",
    url = 'https://github.com/DEMCON/libstored',
    license = 'LGPL3',
	packages = setuptools.find_packages(where=here),
    install_requires = [
        'pyside2',
        'pyserial',
        'argparse',
        'pyzmq',
    ],
    python_requires='>=3.6',
)

