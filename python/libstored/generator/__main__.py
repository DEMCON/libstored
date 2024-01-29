#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import argparse
import datetime
import hashlib
import jinja2
import logging
import re
import shutil
import struct
import uuid
from functools import reduce

import sys
import os
import os.path
from textx import metamodel_from_file
from textx.export import metamodel_export, model_export

from .dsl import types
from ..version import libstored_version

generator_dir = os.path.dirname(__file__)

# We are either installed by pip, so the sources are at generator_dir/../data,
# or we are just running from the git repo, which is at generator_dir/../../..
libstored_dir = os.path.normpath(os.path.abspath(os.path.join(generator_dir, '..', 'data')))
if not os.path.isdir(libstored_dir) or os.path.isfile(os.path.join(libstored_dir, 'ignore')):
    libstored_dir = os.path.normpath(os.path.abspath(os.path.join(generator_dir, '..', '..', '..')))

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

def is_string(o):
    return o.type == 'string'

def is_pointer(o):
    return o.type in ['ptr32', 'ptr64']

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

def qtype(o):
    return {
            'bool': 'bool',
            'int8': 'int',
            'uint8': 'uint',
            'int16': 'int',
            'uint16': 'uint',
            'int32': 'int',
            'uint32': 'uint',
            'int64': 'qlonglong',
            'uint64': 'qulonglong',
            'float': 'float',
            'double': 'double',
            'ptr32': None,
            'ptr64': None,
            'blob': None,
            'string': 'QString'
    }[o.type]

def is_qml_compatible(o):
    return qtype(o) is not None

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
            'double': 'std_logic_vector(63 downto 0)',
            'ptr32': 'std_logic_vector(31 downto 0)',
            'ptr64': 'std_logic_vector(63 downto 0)',
            'blob': 'std_logic_vector(%d downto 0)' % (o.size * 8 - 1),
            'string': 'std_logic_vector(%d downto 0)' % (o.size * 8 - 1),
    }[o.type]

def vhdlinit(o):
    b = lambda: 'x"' + (('00' * o.size) + reduce(lambda a, b: '%02x' % b + a, o.encode(o.init, False)))[-o.size * 2:] + '"'

    b = None
    if o.init != None:
        b = ('00' * o.size)
        b += reduce(lambda a, b: a + ('%02x' % b), o.encode(o.init, False), '')
        b = f'x"{b[-o.size * 2:]}"'

    return {
            'bool': "(7 downto 0 => '0')" if o.init == None or b == 'x"00"' else "(7 downto 1 => '0', 0 => '1')",
            'int8': "(7 downto 0 => '0')" if o.init == None else b,
            'uint8': "(7 downto 0 => '0')" if o.init == None else b,
            'int16': "(15 downto 0 => '0')" if o.init == None else b,
            'uint16': "(15 downto 0 => '0')" if o.init == None else b,
            'int32': "(31 downto 0 => '0')" if o.init == None else b,
            'uint32': "(31 downto 0 => '0')" if o.init == None else b,
            'int64': "(63 downto 0 => '0')" if o.init == None else b,
            'uint64': "(63 downto 0 => '0')" if o.init == None else b,
            'float': "(31 downto 0 => '0')" if o.init == None else b,
            'double': "(63 downto 0 => '0')" if o.init == None else b,
            'ptr32': "(31 downto 0 => '0')" if o.init == None else b,
            'ptr64': "(63 downto 0 => '0')" if o.init == None else b,
            'blob': "(%d downto 0 => '0')" % (o.size * 8 - 1) if o.init == None else b,
            'string': "(%d downto 0 => '0')" % (o.size * 8 - 1) if o.init == None else b,
    }[o.type]

def vhdlstr(s):
    return '(' + ', '.join(map(lambda c: 'x"%02x"' % c, s.encode())) + ')'

def vhdlkey(o, store, littleEndian):
    key = struct.pack(('<' if littleEndian else '>') + 'I', o.offset)
    if store.buffer.size >= 0x1000000:
        pass
    elif store.buffer.size >= 0x10000:
        if littleEndian:
            key = key[:3]
        else:
            key = key[1:]
    elif store.buffer.size >= 0x100:
        if littleEndian:
            key = key[:2]
        else:
            key = key[2:]
    else:
        if littleEndian:
            key = key[:1]
        else:
            key = key[3:]

    return 'x"' + ''.join(map(lambda x: '%02x' % x, key)) + '"'

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

def csvstring(s):
    needEscape = False
    for c in ['\r','\n','"',',']:
        if c in s:
            needEscape = True

    if not needEscape:
        return s

    return '"' + re.sub(r'"', r'""', s) + '"'

def pystring(s):
    return repr(str(s))

def pyliteral(x):
    if isinstance(x, float):
        return f'float(\'{x}\')'
    elif isinstance(x, bool):
        return 'True' if x else 'False'
    elif isinstance(x, int):
        return f'int({x})'
    else:
        return repr(x)

def tab_indent(s, num):
    return ('\t' * num).join(s.splitlines(True))

def model_name(model_file):
    return os.path.splitext(os.path.split(model_file)[1])[0]

def model_cname(model_file):
    s = model_name(model_file)
    s = re.sub(r'[^A-Za-z0-9]+', '_', s)
    s = re.sub(r'_([a-z])', lambda m: m.group(1).upper(), s)
    s = re.sub(r'^_|_$', '', s)
    s = s[0].upper() + s[1:]
    return s

def platform_win32():
    return sys.platform == 'win32'

def spdx(license='MPL-2.0', prefix=''):
    # REUSE-IgnoreStart
    return \
        f'{prefix}SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers\n' + \
        f'{prefix}\n' + \
        f'{prefix}SPDX-License-Identifier: {license}\n'
    # REUSE-IgnoreEnd

def sha1(file):
    with open(file, 'rb') as f:
        return hashlib.sha1(f.read()).hexdigest()

model_names = set()
model_cnames = set()
models = set()

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
            types.BlobType, types.StringType, types.Immediate
        ],
        debug=debug)

    mname = model_name(filename)
    if mname in model_names:
        logger.critical(f'Model {mname} already exists')
        sys.exit(1)

    model_names.add(mname)

    mcname = model_cname(filename)
    if mcname in model_cnames:
        logger.critical(f'Model {mname}\'s class name {mcname} is ambiguous')
        sys.exit(1)

    model_cnames.add(mcname)

    model = meta.model_from_file(filename, debug=debug)
    model.stname = os.path.split(filename)[1]
    model.filename = mname
    model.name = mcname
    model.littleEndian = littleEndian
    model.process()
    models.add(model)
    return model

def generate_store(model_file, output_dir, littleEndian=True):
    logger.info(f"generating store {model_name(model_file)}")

    model = load_model(model_file, littleEndian)
    mname = model_name(model_file)

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

    jenv.globals['store'] = model
    jenv.globals['win32'] = platform_win32()
    jenv.filters['ctype'] = ctype
    jenv.filters['qtype'] = qtype
    jenv.filters['stype'] = stype
    jenv.filters['vhdltype'] = vhdltype
    jenv.filters['vhdlinit'] = vhdlinit
    jenv.filters['vhdlstr'] = vhdlstr
    jenv.filters['vhdlkey'] = vhdlkey
    jenv.filters['cname'] = types.cname
    jenv.filters['carray'] = carray
    jenv.filters['vhdlname'] = types.vhdlname
    jenv.filters['len'] = len
    jenv.filters['hasfunction'] = has_function
    jenv.filters['rtfstring'] = rtfstring
    jenv.filters['csvstring'] = csvstring
    jenv.filters['pystring'] = pystring
    jenv.filters['pyliteral'] = pyliteral
    jenv.filters['tab_indent'] = tab_indent
    jenv.tests['variable'] = is_variable
    jenv.tests['function'] = is_function
    jenv.tests['blob'] = is_blob
    jenv.tests['string'] = is_string
    jenv.tests['pointer'] = is_pointer
    jenv.tests['qml_compatible'] = is_qml_compatible

    store_h_tmpl = jenv.get_template('store.h.tmpl')
    store_cpp_tmpl = jenv.get_template('store.cpp.tmpl')
    store_rtf_tmpl = jenv.get_template('store.rtf.tmpl')
    store_csv_tmpl = jenv.get_template('store.csv.tmpl')
    store_py_tmpl = jenv.get_template('store.py.tmpl')
    store_vhd_tmpl = jenv.get_template('store.vhd.tmpl')
    store_pkg_vhd_tmpl = jenv.get_template('store_pkg.vhd.tmpl')

    with open(os.path.join(output_dir, 'include', mname + '.h'), 'w') as f:
        f.write(store_h_tmpl.render())

    with open(os.path.join(output_dir, 'src', mname + '.cpp'), 'w') as f:
        f.write(store_cpp_tmpl.render())

    with open(os.path.join(output_dir, 'doc', mname + '.rtf'), 'w') as f:
        f.write(store_rtf_tmpl.render())

    with open(os.path.join(output_dir, 'doc', mname + '.rtf.license'), 'w') as f:
        f.write(spdx('CC0-1.0'))

    with open(os.path.join(output_dir, 'doc', mname + '.csv'), 'w') as f:
        f.write(store_csv_tmpl.render())

    with open(os.path.join(output_dir, 'doc', mname + '.csv.license'), 'w') as f:
        f.write(spdx('CC0-1.0'))

    with open(os.path.join(output_dir, 'doc', mname + 'Meta.py'), 'w') as f:
        f.write(store_py_tmpl.render())

    with open(os.path.join(output_dir, 'rtl', mname + '.vhd'), 'w') as f:
        f.write(store_vhd_tmpl.render())

    with open(os.path.join(output_dir, 'rtl', mname + '_pkg.vhd'), 'w') as f:
        f.write(store_pkg_vhd_tmpl.render())

    licenses_dir = os.path.join(output_dir, 'LICENSES')
    os.makedirs(licenses_dir, exist_ok=True)
    shutil.copy(os.path.join(libstored_dir, 'LICENSES', 'CC0-1.0.txt'), licenses_dir)
    shutil.copy(os.path.join(libstored_dir, 'LICENSES', 'MPL-2.0.txt'), licenses_dir)

def generate_cmake(libprefix, model_files, output_dir):
    logger.info("generating CMakeLists.txt")
    model_map = list(map(model_name, model_files))

    try:
        libstored_reldir = '${CMAKE_CURRENT_SOURCE_DIR}/' + os.path.relpath(libstored_dir, output_dir)
    except:
        libstored_reldir = libstored_dir

    # create the output dir if it does not exist yet
    if not os.path.exists(output_dir):
        os.mkdir(output_dir)

    jenv = jinja2.Environment(
            loader = jinja2.FileSystemLoader([
                os.path.join(libstored_dir),
                os.path.join(libstored_dir, 'doc'),
                os.path.join(libstored_dir, 'fpga', 'vivado'),
            ]),
            trim_blocks = True,
            lstrip_blocks = True)

    jenv.filters['header'] = lambda m: f'include/{m}.h'
    jenv.filters['src'] = lambda m: f'src/{m}.cpp'
    jenv.filters['escapebs'] = escapebs
    jenv.globals['sha1'] = lambda f: sha1(os.path.join(output_dir, f))

    cmake_tmpl = jenv.get_template('CMakeLists.txt.tmpl')
    vivado_tmpl = jenv.get_template('vivado.tcl.tmpl')
    spdx_tmpl = jenv.get_template('libstored-src.spdx.tmpl')
    sha1sum_tmpl = jenv.get_template('SHA1SUM.tmpl')

    with open(os.path.join(output_dir, 'CMakeLists.txt'), 'w') as f:
        f.write(cmake_tmpl.render(
            libstored_dir=libstored_reldir,
            models=model_map,
            libprefix=libprefix,
            python_executable=sys.executable,
            ))

    with open(os.path.join(output_dir, 'rtl', 'vivado.tcl'), 'w') as f:
        f.write(vivado_tmpl.render(
            libstored_dir=libstored_dir,
            models=model_map,
            libprefix=libprefix,
            ))

    with open(os.path.join(output_dir, 'doc', 'SHA1SUM'), 'w') as f:
        f.write(sha1sum_tmpl.render(
            libstored_dir=libstored_dir,
            models=models,
            libprefix=libprefix,
            ))

    with open(os.path.join(output_dir, 'doc', 'SHA1SUM.license'), 'w') as f:
        f.write(spdx('CC0-1.0'))

    with open(os.path.join(output_dir, 'doc', 'libstored-src.spdx'), 'w') as f:
        f.write(spdx_tmpl.render(
            libstored_dir=libstored_dir,
            models=models,
            libprefix=libprefix,
            libstored_version=libstored_version,
            uuid=str(uuid.uuid4()),
            timestamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
            ))

def main():
    parser = argparse.ArgumentParser(description='Store generator')
    parser.add_argument('-p', type=str, help='libstored prefix for cmake library target')
    parser.add_argument('store_file', type=str, nargs='+', help='store description to parse')
    parser.add_argument('output_dir', type=str, help='output directory for generated files')
    parser.add_argument('-b', help='generate for big-endian device (default=little)', action='store_true')

    args = parser.parse_args()
    for f in args.store_file:
        generate_store(f, args.output_dir, not args.b)

    generate_cmake(args.p, args.store_file, args.output_dir)

if __name__ == '__main__':
    main()
