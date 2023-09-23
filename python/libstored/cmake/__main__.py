#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import argparse
import jinja2
import logging
import os
import re
import sys

from ..version import __version__

logger = logging.getLogger('cmake')

# We are either installed by pip, so the sources are at here/../data,
# or we are just running from the git repo, which is at here/../../..
here = os.path.dirname(__file__)
libstored_dir = os.path.normpath(os.path.abspath(os.path.join(here, '..', 'data')))
if not os.path.isdir(libstored_dir) or os.path.isfile(os.path.join(libstored_dir, 'ignore')):
    libstored_dir = os.path.normpath(os.path.abspath(os.path.join(here, '..', '..', '..')))

def escapebs(s):
    return re.sub(r'\\', r'\\\\', s)

def escapestr(s):
    return re.sub(r'([\\"])', r'\\\1', s)

def generate_cmake(filename, defines):
    jenv = jinja2.Environment(
            loader = jinja2.FileSystemLoader(
                os.path.join(libstored_dir, 'cmake')
                ))

    jenv.filters['escapebs'] = escapebs
    jenv.filters['escapestr'] = escapestr

    tmpl = jenv.get_template('FindLibstored.cmake.tmpl')

    logger.info('Writing to %s...', filename)
    with open(filename, 'w') as f:
        f.write(tmpl.render(
            python_executable=sys.executable,
            libstored_dir=libstored_dir,
            defines=defines,
            ))

def main():
    parser = argparse.ArgumentParser(prog=sys.modules[__name__].__package__,
            description='Generator for find_package(Libstored) in CMake',
            formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument('-v', dest='verbose', default=0, help='enable verbose output', action='count')
    parser.add_argument('-D', dest='define', metavar='key[=value]', default=[], nargs=1, action='append', help='CMake defines')
    parser.add_argument('--version', action='version', version=f'{__version__}')
    parser.add_argument('filename', default='FindLibstored.cmake', nargs='?',
            type=str, help='Output filename')
    args = parser.parse_args()

    if args.verbose == 0:
        logging.basicConfig(level=logging.WARN)
    elif args.verbose == 1:
        logging.basicConfig(level=logging.INFO)
    else:
        logging.basicConfig(level=logging.DEBUG)

    defines = {}
    if args.define is not None:
        for d in args.define:
            kv = d[0].split('=', 1) + ['ON']
            defines[kv[0]] = kv[1]

    generate_cmake(args.filename, defines)

if __name__ == '__main__':
    main()
