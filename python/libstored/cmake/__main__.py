#!/usr/bin/env python3

# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2023  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import argparse
import jinja2
import logging
import os
import re
import sys

from ..version import __version__

logger = logging.getLogger('cmake')

def find_tmpl(paths, file):
    if isinstance(file, list):
        file = os.path.join(*file)

    for p in paths:
        if isinstance(p, list):
            p = os.path.normpath(os.path.join(*p))

        f = os.path.normpath(os.path.join(p, file))
        logger.debug('Trying %s...', f)

        if os.path.isfile(f):
            return (p, os.path.normpath(f))

    return None

def escapebs(s):
    return re.sub(r'\\', r'\\\\', s)

def escapestr(s):
    return re.sub(r'([\\"])', r'\\\1', s)

def generate_cmake(filename, defines):
    here = os.path.dirname(__file__)

    tmpl_name = 'FindLibstored.cmake.tmpl'
    p = find_tmpl([[here, '..', 'src'], [here, '..', '..', '..']], ['cmake', tmpl_name])
    if p is None:
        logger.error('Cannot find template')
        sys.exit(1)

    logger.info('Using %s as template', p[1])

    jenv = jinja2.Environment(
            loader = jinja2.FileSystemLoader(
                os.path.join(p[0], 'cmake')
                ))

    jenv.filters['escapebs'] = escapebs
    jenv.filters['escapestr'] = escapestr

    tmpl = jenv.get_template(tmpl_name)

    logger.info('Writing to %s...', filename)
    with open(filename, 'w') as f:
        f.write(tmpl.render(
            python_executable=sys.executable,
            libstored_dir=p[0],
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
