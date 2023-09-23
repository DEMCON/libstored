#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MIT

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
