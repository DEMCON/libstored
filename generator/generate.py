#!/usr/bin/env python3
# vim:et

import logging
import argparse
import jinja2
import re

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

def is_blob(o):
    return isinstance(o.type, types.BlobType)

def ctype(o):
    if is_blob(o):
        return ctype(o.type)

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
            'ptr': 'void*',
            'blob': 'char',
            'string': 'char'
    }[o.type]

def stype(o):
    if is_blob(o):
        t = stype(o.type)
    else:
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
            'ptr': 'Type::Pointer',
            'blob': 'Type::Blob',
            'string': 'Type::String'
        }[o.type]

    if is_function(o):
        t += ' | Type::Function'
    return t

def carray(a):
    s = ''
    line = 0
    for i in a:
        s += '0x%02x, ' % i
        line += 1
        if line > 32:
            s += '\n'
            line = 0
    return s

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
def load_model(filename, debug=False):
    meta = metamodel_from_file(
        os.path.join(generator_dir, 'dsl', 'grammar.tx'),
        classes=[types.Store,
            types.Variable, types.Function,
            types.BlobType
        ],
        debug=debug)

    
    model = meta.model_from_file(filename, debug=False)
    model.name = model_cname(filename)
    return model

def generate_store(model_file, output_dir, debug=False):
    logger.info(f"generating store {model_name(model_file)}")

    model = load_model(model_file)

    # create the output dir if it does not exist yet
    if not os.path.exists(output_dir):
        os.mkdir(output_dir)
    if not os.path.exists(os.path.join(output_dir, 'include')):
        os.mkdir(os.path.join(output_dir, 'include'))
    if not os.path.exists(os.path.join(output_dir, 'src')):
        os.mkdir(os.path.join(output_dir, 'src'))

    # now generate the code

    jenv = jinja2.Environment(
            loader = jinja2.FileSystemLoader([
                os.path.join(libstored_dir, 'include', 'libstored'),
                os.path.join(libstored_dir, 'src'),
            ]),
            trim_blocks = True,
            lstrip_blocks = True)

    jenv.filters['ctype'] = ctype
    jenv.filters['stype'] = stype
    jenv.filters['cname'] = types.cname
    jenv.filters['carray'] = carray
    jenv.filters['len'] = len
    jenv.tests['variable'] = is_variable
    jenv.tests['function'] = is_function
    jenv.tests['blob'] = is_blob

    store_h_tmpl = jenv.get_template('store.h.tmpl')
    store_cpp_tmpl = jenv.get_template('store.cpp.tmpl')

    with open(os.path.join(output_dir, 'include', model_name(model_file) + '.h'), 'w') as f:
        f.write(store_h_tmpl.render(store=model))

    with open(os.path.join(output_dir, 'src', model_name(model_file) + '.cpp'), 'w') as f:
        f.write(store_cpp_tmpl.render(store=model))

def generate_cmake(libprefix, model_files, output_dir, debug=False):
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

    args = parser.parse_args()
    for f in args.store_file:
        generate_store(f, args.output_dir)

    generate_cmake(args.p, args.store_file, args.output_dir)

