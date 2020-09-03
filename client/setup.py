#!/usr/bin/env python3
# vim:et

import sys
import os
import setuptools
import ed2

here = os.path.dirname(os.path.realpath(__file__))

#with open(os.path.join(here, "README.md"), "r") as fh:
#    long_description = fh.read()

setuptools.setup(
	name = 'ed2',
	version = ed2.__version__,
    description = 'Embedded Debugger protocol as used by libstored',
    author = 'Jochem Rutgers',
    author_email = 'jochem.rutgers@demcon.com',
#    long_description=long_description,
#    long_description_content_type="text/markdown",
    url = 'https://github.com/DEMCON/libstored',
    license = 'LGPL3+',
	packages = setuptools.find_packages(where=here),
    install_requires = [
        'pyside2',
        'pyserial',
        'argparse',
        'pyzmq',
        'lognplot',
        'crcmod',
    ],
    python_requires='>=3.6',
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: GNU Lesser General Public License v3 or later (LGPLv3+)",
        "Intended Audience :: Developers",
        "Topic :: Software Development :: Debuggers",
        "Topic :: Software Development :: Embedded Systems",
    ],
)

