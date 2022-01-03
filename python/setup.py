#!/usr/bin/env python3

import os
import re
import setuptools
import sys
import libstored

here = os.path.dirname(os.path.realpath(__file__))

with open(os.path.join(here, "README.md"), "r") as fh:
    long_description = fh.read()

packages = setuptools.find_packages(here)
packages += list(map(lambda p: re.sub(r'^libstored\b', 'ed2', p), packages))
package_data = [
]

setuptools.setup(
    name = 'libstored',
    version = libstored.__version__,
    description = 'Embedded Debugger client for libstored\'s debug protocol',
    author = 'Jochem Rutgers',
    author_email = 'jochem.rutgers@demcon.com',
    long_description=long_description,
    long_description_content_type="text/markdown",
    url = 'https://github.com/DEMCON/libstored',
    license = 'LGPL3+',
    packages = packages,
    package_dir = {'libstored': 'libstored', 'ed2': 'libstored'},
    package_data = {'libstored': package_data, 'ed2': package_data},
    install_requires = [
        'PySide6',
        'pyserial',
        'argparse',
        'pyzmq',
        'crcmod',
        'natsort',
        'heatshrink2',
        'matplotlib>=3.5.0', # PySide6 support starts from 3.5.0
#        'lognplot', # optional
    ],
    python_requires='>=3.6',
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: GNU Lesser General Public License v3 or later (LGPLv3+)",
        "Intended Audience :: Developers",
        "Topic :: Software Development :: Debuggers",
        "Topic :: Software Development :: Embedded Systems",
    ],
    include_package_data=True,
)

