#!/usr/bin/env python3
# vim:et

# libstored, a Store for Embedded Debugger.
# Copyright (C) 2020  Jochem Rutgers
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

import logging
import argparse
import jinja2
import re
import hashlib

import sys
import os
import os.path
from textx import metamodel_from_file
from textx.export import metamodel_export, model_export

from dsl import types

generator_dir = os.path.dirname(__file__)
libstored_dir = os.path.abspath(os.path.join(generator_dir, '..'))

logging.basicConfig(format='       %(message)s', level=logging.DEBUG)
logger = logging.getLogger('libstored')

def is_variable(o):
    return isinstance(o, types.Variable)

def is_function(o):
    return isinstance(o, types.Function)

def has_function(os):
    for o in os:
        if is_function(o):
            return True
    return False

def is_blob(o):
    return o.isBlob()

def is_pointer(o):
    return o.type in ['ptr32', 'ptr64'];

def ctype(o):
    return {
            'bool': 'bool',
            'int8': 'int8_t',
            'uint8': 'uint8_t',
            'int16': 'int16_t',
            'uint16': 'uint16_t',
            'int32': 'int32_t',
            'uint32': 'uint32_t',
            'int64': 'int64_t',
            'uint64': 'uint64_t',
            'float': 'float',
            'double': 'double',
            'ptr32': 'void*',
            'ptr64': 'void*',
            'blob': 'void',
            'string': 'char'
    }[o.type]

def stype(o):
    t = {
        'bool': 'Type::Bool',
        'int8': 'Type::Int8',
        'uint8': 'Type::Uint8',
        'int16': 'Type::Int16',
        'uint16': 'Type::Uint16',
        'int32': 'Type::Int32',
        'uint32': 'Type::Uint32',
        'int64': 'Type::Int64',
        'uint64': 'Type::Uint64',
        'float': 'Type::Float',
        'double': 'Type::Double',
        'ptr32': 'Type::Pointer',
        'ptr64': 'Type::Pointer',
        'blob': 'Type::Blob',
        'string': 'Type::String'
    }[o.type]

    if is_function(o):
        t += ' | Type::FlagFunction'
    return t

def vhdltype(o):
    return {
            'bool': 'std_logic',
            'int8': 'signed(7 downto 0)',
            'uint8': 'unsigned(7 downto 0)',
            'int16': 'signed(15 downto 0)',
            'uint16': 'unsigned(15 downto 0)',
            'int32': 'signed(31 downto 0)',
            'uint32': 'unsigned(31 downto 0)',
            'int64': 'signed(63 downto 0)',
            'uint64': 'unsigned(63 downto 0)',
            'float': 'std_logic_vector(31 downto 0)',
            'double': 'std_logic_vector(64 downto 0)',
            'ptr32': 'std_logic_vector(31 downto 0)',
            'ptr64': 'std_logic_vector(63 downto 0)',
            'blob': 'std_logic_vector(%d downto 0)' % (o.size - 1),
            'string': 'std_logic_vector(%d downto 0)' % (o.size - 1),
    }[o.type]

def vhdlinit(o):
    return {
            'bool': "'0'",
            'int8': "(7 downto 0 => '0')",
            'uint8': "(7 downto 0 => '0')",
            'int16': "(15 downto 0 => '0')",
            'uint16': "(15 downto 0 => '0')",
            'int32': "(31 downto 0 => '0')",
            'uint32': "(31 downto 0 => '0')",
            'int64': "(63 downto 0 => '0')",
            'uint64': "(63 downto 0 => '0')",
            'float': "(31 downto 0 => '0')",
            'double': "(64 downto 0 => '0')",
            'ptr32': "(31 downto 0 => '0')",
            'ptr64': "(64 downto 0 => '0')",
            'blob': "(%d downto 0 => '0')" % (o.size - 1),
            'string': "(%d downto 0 => '0')" % (o.size - 1),
    }[o.type]

def vhdlstr(s):
    return '(' + ', '.join(map(lambda c: 'x"%02x"' % c, s.encode())) + ')'

def carray(a):
    s = ''
    line = 0
    for i in a:
        s += '0x%02x, ' % i
        line += 1
        if line >= 16:
            s += '\n'
            line = 0
    return s

def escapebs(s):
    return re.sub(r'\\', r'\\\\', s)

def rtfstring(s):
    return re.sub(r'([\\{}])', r'\\\1', s)

def model_name(model_file):
    return os.path.splitext(os.path.split(model_file)[1])[0]

def model_cname(model_file):
    s = model_name(model_file)
    s = re.sub(r'[^A-Za-z0-9]+', '_', s)
    s = re.sub(r'_([a-z])', lambda m: m.group(1).upper(), s)
    s = re.sub(r'^_|_$', '', s)
    s = s[0].upper() + s[1:]
    return s

##
# @brief Load a model from a file
# @param filename The name of the file to load
# @param debug True to output additional debug information, false otherwise
#
def load_model(filename, littleEndian=True, debug=False):
    meta = metamodel_from_file(
        os.path.join(generator_dir, 'dsl', 'grammar.tx'),
        classes=[types.Store,
            types.Variable, types.Function, types.Scope,
            types.BlobType, types.Immediate
        ],
        debug=debug)


    model = meta.model_from_file(filename, debug=debug)
    model.name = model_cname(filename)
    model.littleEndian = littleEndian
    model.process()
    return model

def generate_store(model_file, output_dir, littleEndian=True):
    logger.info(f"generating store {model_name(model_file)}")

    model = load_model(model_file, littleEndian)

    with open(model_file, 'rb') as f:
        model.hash = hashlib.sha1(f.read().replace(b'\r\n', b'\n')).hexdigest()

    # create the output dir if it does not exist yet
    if not os.path.exists(output_dir):
        os.mkdir(output_dir)
    if not os.path.exists(os.path.join(output_dir, 'include')):
        os.mkdir(os.path.join(output_dir, 'include'))
    if not os.path.exists(os.path.join(output_dir, 'src')):
        os.mkdir(os.path.join(output_dir, 'src'))
    if not os.path.exists(os.path.join(output_dir, 'doc')):
        os.mkdir(os.path.join(output_dir, 'doc'))
    if not os.path.exists(os.path.join(output_dir, 'rtl')):
        os.mkdir(os.path.join(output_dir, 'rtl'))

    # now generate the code

    jenv = jinja2.Environment(
            loader = jinja2.FileSystemLoader([
                os.path.join(libstored_dir, 'include', 'libstored'),
                os.path.join(libstored_dir, 'src'),
                os.path.join(libstored_dir, 'doc'),
                os.path.join(libstored_dir, 'fpga', 'rtl'),
            ]),
            trim_blocks = True,
            lstrip_blocks = True)

    jenv.filters['ctype'] = ctype
    jenv.filters['stype'] = stype
    jenv.filters['vhdltype'] = vhdltype
    jenv.filters['vhdlinit'] = vhdlinit
    jenv.filters['vhdlstr'] = vhdlstr
    jenv.filters['cname'] = types.cname
    jenv.filters['carray'] = carray
    jenv.filters['len'] = len
    jenv.filters['hasfunction' ] = has_function
    jenv.filters['rtfstring' ] = rtfstring
    jenv.tests['variable'] = is_variable
    jenv.tests['function'] = is_function
    jenv.tests['blob'] = is_blob
    jenv.tests['pointer'] = is_pointer

    store_h_tmpl = jenv.get_template('store.h.tmpl')
    store_cpp_tmpl = jenv.get_template('store.cpp.tmpl')
    store_rtf_tmpl = jenv.get_template('store.rtf.tmpl')
    store_vhd_tmpl = jenv.get_template('store.vhd.tmpl')
    store_pkg_vhd_tmpl = jenv.get_template('store_pkg.vhd.tmpl')

    with open(os.path.join(output_dir, 'include', model_name(model_file) + '.h'), 'w') as f:
        f.write(store_h_tmpl.render(store=model))

    with open(os.path.join(output_dir, 'src', model_name(model_file) + '.cpp'), 'w') as f:
        f.write(store_cpp_tmpl.render(store=model))

    with open(os.path.join(output_dir, 'doc', model_name(model_file) + '.rtf'), 'w') as f:
        f.write(store_rtf_tmpl.render(store=model))

    with open(os.path.join(output_dir, 'rtl', model_name(model_file) + '.vhd'), 'w') as f:
        f.write(store_vhd_tmpl.render(store=model))

    with open(os.path.join(output_dir, 'rtl', model_name(model_file) + '_pkg.vhd'), 'w') as f:
        f.write(store_pkg_vhd_tmpl.render(store=model))

def generate_cmake(libprefix, model_files, output_dir):
    logger.info("generating CMakeLists.txt")
    models = map(model_name, model_files)

    # create the output dir if it does not exist yet
    if not os.path.exists(output_dir):
        os.mkdir(output_dir)

    jenv = jinja2.Environment(
            loader = jinja2.FileSystemLoader([
                os.path.join(libstored_dir),
            ]),
            trim_blocks = True,
            lstrip_blocks = True)

    jenv.filters['header'] = lambda m: f'include/{m}.h'
    jenv.filters['src'] = lambda m: f'src/{m}.cpp'
    jenv.filters['escapebs'] = escapebs

    cmake_tmpl = jenv.get_template('CMakeLists.txt.tmpl')

    with open(os.path.join(output_dir, 'CMakeLists.txt'), 'w') as f:
        f.write(cmake_tmpl.render(
            libstored_dir=libstored_dir,
            models=models,
            libprefix=libprefix,
            ))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Store generator')
    parser.add_argument('-p', type=str, help='libstored prefix for cmake library target')
    parser.add_argument('store_file', type=str, nargs='+', help='store description to parse')
    parser.add_argument('output_dir', type=str, help='output directory for generated files')
    parser.add_argument('-b', help='generate for big-endian device (default=little)', action='store_true')

    args = parser.parse_args()
    for f in args.store_file:
        generate_store(f, args.output_dir, not args.b)

    generate_cmake(args.p, args.store_file, args.output_dir)

