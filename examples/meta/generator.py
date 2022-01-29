#!/usr/bin/env python3

# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import argparse
import importlib
import jinja2
import os
import sys

def cstr(s):
    bs = str(s).encode()
    cs = '"'
    for b in bs:
        if b < 32 or b >= 127:
            cs += f'\\x{b:02x}'
        else:
            cs += chr(b)
    return cs + '"'

def generate(meta, tmpl_filename, output_filename):
    output_dir = os.path.dirname(output_filename)
    if not os.path.exists(output_dir):
        os.mkdir(output_dir)

    jenv = jinja2.Environment(
            loader = jinja2.FileSystemLoader(os.path.dirname(tmpl_filename)),
            trim_blocks=True)

    jenv.filters['cstr'] = cstr

    tmpl = jenv.get_template(os.path.basename(tmpl_filename))

    with open(os.path.join(output_filename), 'w') as f:
        f.write(tmpl.render(store=meta))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generator using store meta data')
    parser.add_argument('-m', '--meta', type=str, required=True, help='path to <store>Meta.py as input', dest="meta")
    parser.add_argument('-t', '--template', type=str, required=True, help='path to jinja2 template META is to be applied to', dest="template")
    parser.add_argument('-o', '--output', type=str, required=True, help='output file for jinja2 generated content', dest="output")

    args = parser.parse_args()

    module_name = os.path.splitext(os.path.basename(args.meta))[0]
    with open(args.meta) as f:
        exec(f.read())
    meta = eval(f'{module_name}()')

    generate(meta, args.template, args.output)
