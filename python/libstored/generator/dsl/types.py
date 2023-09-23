# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import re
import struct
import copy
import sys

cnames = {}

def is_reserved_name(s):
    return s in [
        # C++
        'alignas', 'alignof', 'and', 'and_eq', 'asm', 'atomic_cancel',
        'atomic_commit', 'atomic_noexcept', 'auto', 'bitand', 'bitor', 'bool',
        'break', 'case', 'catch', 'char', 'char8_t', 'char16_t', 'char32_t',
        'class', 'compl', 'concept', 'const', 'consteval', 'constexpr',
        'constinit', 'const_cast', 'continue', 'co_await', 'co_return',
        'co_yield', 'decltype', 'default', 'delete', 'do', 'double',
        'dynamic_cast', 'else', 'enum', 'explicit', 'export', 'extern',
        'false', 'float', 'for', 'friend', 'goto', 'if', 'inline', 'int',
        'long', 'mutable', 'namespace', 'new', 'noexcept', 'not', 'not_eq',
        'nullptr', 'operator', 'or', 'or_eq', 'private', 'protected', 'public',
        'reflexpr', 'register', 'reinterpret_cast', 'requires', 'return',
        'short', 'signed', 'sizeof', 'static', 'static_assert', 'static_cast',
        'struct', 'switch', 'synchronized', 'template', 'this', 'thread_local',
        'throw', 'true', 'try', 'typedef', 'typeid', 'typename', 'union',
        'unsigned', 'using', 'virtual', 'void', 'volatile', 'wchar_t', 'while',
        'xor', 'xor_eq',
        # C++ non-keywords, but still tricky to use
        'override', 'final',
        # C
        'auto', 'break', 'case', 'char', 'const', 'continue', 'default', 'do',
        'double', 'else', 'enum', 'extern', 'float', 'for', 'goto', 'if',
        'inline', 'int', 'long', 'register', 'restrict', 'return', 'short',
        'signed', 'sizeof', 'static', 'struct', 'switch', 'typedef', 'union',
        'unsigned', 'void', 'volatile', 'while', '_Alignas', '_Alignof',
        '_Atomic', '_Bool', '_Complex', '_Generic', '_Imaginary', '_Noreturn',
        '_Static_assert', '_Thread_local',
    ]

def cname(s):
    if s in cnames:
        return cnames[s]
    c = s
    c = re.sub(r'[^A-Za-z0-9/]+', '_', c)
    c = re.sub(r'_*/+', '__', c)
    c = re.sub(r'^__', '', c)
    c = re.sub(r'^[^A-Za-z]_*', '_', c)
    c = re.sub(r'_+$', '', c)

    if s == '':
        c = 'obj'

    if is_reserved_name(c):
        c += "_obj"

    u = c
    i = 2
    while u in cnames.values():
        u = c + f'_{i}'
        i += 1

    cnames[s] = u
    return u

vhdlnames = {}

def vhdlname(s):
    s = str(s)
    if s in vhdlnames:
        return vhdlnames[s]
    c = s
    c = re.sub(r'\\', '\\\\', c)

    if s == '':
        c = 'obj'

    u = c
    i = 2
    while u in vhdlnames.values():
        u = c + f' {i}'
        i += 1

    vhdlnames[s] = u
    return u

def csize(o):
    return {
            'bool': 1,
            'int8': 1,
            'uint8': 1,
            'int16': 2,
            'uint16': 2,
            'int32': 4,
            'uint32': 4,
            'int64': 8,
            'uint64': 8,
            'float': 4,
            'double': 8,
            'ptr32': 4,
            'ptr64': 8,
            'blob': 0,
            'string': 0,
    }[o]

def typeflags(s, func=False):
    return {
            'bool': 0x20,
            'int8': 0x38,
            'uint8': 0x30,
            'int16': 0x39,
            'uint16': 0x31,
            'int32': 0x3b,
            'uint32': 0x33,
            'int64': 0x3f,
            'uint64': 0x37,
            'float': 0x2b,
            'double': 0x2f,
            'ptr32': 0x23,
            'ptr64': 0x27,
            'blob': 0x01,
            'string': 0x02,
    }[s] + (0x40 if func else 0)

def object_name(s):
    s = re.sub(r'\s+', ' ', s)
    return s

class Directory(object):
    def __init__(self):
        self.data = []
        self.longdata = []

    def merge(self, root, h):
#        print(f'merge {root} {h}')
        for k,v in h.items():
            if k in root:
                self.merge(root[k], v)
            else:
                root[k] = v
#        print(f'merged into {root}')

    def convertHierarchy(self, xs):
        # xs is a list of pairs of ([name chunks], object)
        res = {}
        for x in xs:
#            print(x)
            if len(x[0]) == 1:
                # No sub scope
                res[x[0][0]] = x[1]
            else:
                # First element of x[0] is the subscope
                if x[0][0] not in res:
                    res[x[0][0]] = {}
                self.merge(res[x[0][0]], self.convertHierarchy([(x[0][1:], x[1])]))
        return res

    def hierarchical(self, objects):
        # Extract and sort on name.
        objects = sorted(map(lambda o: (o.name, o), objects), key=lambda x: x[0])

        # Split in hierarchy.
        objects = map(lambda x: (list(x[0] + '\x00'), x[1]), objects)

        # Make hierarchy.
        objects = self.convertHierarchy(objects)

#        print(objects)
        return objects

    def encodeInt(self, i):
        if i == 0:
            return [0]

        res = []
        while i >= 0x80:
            res.insert(0, i & 0x7f)
            i = i >> 7
        res.insert(0, i)

        for j in range(0,len(res)-1):
            res[j] += 0x80

        return res

    def encodeType(self, o):
        func = isinstance(o, Function)
        res = [typeflags(o.type, func) + 0x80]
        if o.isBlob():
            res += self.encodeInt(o.size)
        return res

    def generateDict(self, h):
        if isinstance(h, Variable):
            return self.encodeType(h) + self.encodeInt(h.offset)
        elif isinstance(h, Function):
#            print(f'function {h.name} {h.f}')
            return self.encodeType(h) + self.encodeInt(h.f)
        else:
            assert isinstance(h, dict)

        names = list(h.keys())
        names.sort()
#        print(names)

        if names == []:
            # end
            return [0]
        elif names == ['\x00']:
            return self.generateDict(h['\x00'])
        elif names == ['/']:
            return [ord('/')] + self.generateDict(h['/'])
        elif len(names) == 1 and isinstance(names[0], int):
            # skip
            skip = names[0]
            res = []
            while skip > 0:
                s = min(skip, 0x1f)
                res += [s]
                skip -= s
            return res + self.generateDict(h[names[0]])
        else:
            # Choose pivot to compare to
            pivot = int(len(names) / 2)
            if names[pivot] == '/':
                if pivot > 0:
                    pivot -= 1
                else:
                    pivot += 1
            assert(names[pivot] != '/')
            expr = self.generateDict(h[names[pivot]])

            # Elements less than pivot
            l = {}
            for n in names[0:pivot]:
                l[n] = h[n]
            expr_l = self.generateDict(l)
            if expr_l == [0]:
                expr_l = []

            # Elements greater than pivot
            g = {}
            for n in names[pivot+1:]:
                g[n] = h[n]
            expr_g = self.generateDict(g)
            if expr_g == [0]:
                expr_g = []

            if expr_g == []:
                jmp_g = [0]
            else:
                jmp_g = self.encodeInt(len(expr) + len(expr_l) + 1)

            if expr_l == []:
                jmp_l = [0]
            else:
                jmp_l = self.encodeInt(len(jmp_g) + len(expr) + 1)

            return [ord(names[pivot])] + jmp_l + jmp_g + expr + expr_l + expr_g

    def stripUnambig(self, h):
        if not isinstance(h, dict):
            return
        if len(h) == 1:
            if '/' in h:
                self.stripUnambig(h['/'])
                return
            elif '\x00' in h:
                return

        # Drop at end of name:
        # Find the chain:  h[k] -> v[vk] -> vv={[\0/]: object}
        # And replace by:  h[k] -> vv

        # Skip unambiguous chars within name:
        # Find the chain:  h[k] -> v[vk] -> multiple vv
        # And replace by:  h[k] -> v[1]  -> multiple vv

        # Skip more:
        # Find the chain:  h[k] -> v[vk] -> vv={int: object}
        # And replace by:  h[k] -> vv={int+1: object}

        # Strip off paths with single leaf object:
        # Find the chain: h[k] -> v[/] -> vv={char: {\0: object}}
        # And replace by: k[k] -> vv

#        print(f'strip {h}')
        for k in list(h.keys()):
            v = h[k]
            if not isinstance(v, dict):
                continue
            self.stripUnambig(v)
            if len(v) == 1:
                vk = list(v.keys())[0]
                if vk == '\x00':
                    continue
                vv = v[vk]
                if vk == '/':
                    if len(vv) == 1:
                        # Scope with single leaf?
                        vkk = list(vv.keys())[0]
                        if isinstance(vkk, str) and isinstance(vv[vkk], dict) and len(vv[vkk]) == 1 and '\0' in vv[vkk]:
#                            print(f'drop path of {v}')
                            h[k] = vv[vkk]
                    continue
                if len(vv) == 1:
                    vvk = list(vv.keys())[0]
                    if isinstance(vvk, int):
#                        print(f'skip more {vk}')
                        h[k] = vv
                        vv[vvk + 1] = vv[vvk]
                        del vv[vvk]
                    elif vvk == '/' or vvk == '\x00':
#                        print(f'drop {vk}')
                        h[k] = vv
                else:
#                    print(f'skip {vk}')
                    v[1] = v[vk]
                    del v[vk]
#        print(f'stripped {h}')

    def generate(self, objects):
        h = self.hierarchical(objects)
        self.longdata = [ord('/')] + self.generateDict(h) + [0]
        self.stripUnambig(h)
        self.data = [ord('/')] + self.generateDict(h) + [0]
#        print(self.data)

class Buffer(object):
    def __init__(self):
        self.size = 0
        self.init = []

    def align(self, size, force=None):
        a = 8
        if force != None:
            a = force
        elif size <= 0:
            return size
        elif size <= 1:
            return size
        elif size <= 2:
            a = 2
        elif size <= 4:
            a = 4

        if size % a == 0:
            return size
        else:
            return size + a - (size % a)

    def generate(self, initvars, defaultvars, littleEndian = True):
        self.init = []
        for v in initvars:
            size = v.buffersize()
            alignedsize = self.align(size)
            v.offset = len(self.init)
            self.init += list(v.encode(v.init, littleEndian))
            if alignedsize > size:
                self.init += [0] * (alignedsize - size)

        self.size = self.align(len(self.init), 8)
        for v in defaultvars:
            size = v.buffersize()
            alignedsize = self.align(size)
            v.offset = self.size
            self.size += alignedsize

        if self.size == 0:
            self.size = 1

class ArrayLookup(object):
    def __init__(self, objects):
        # All objects should have the same name, but only differ in their array index.
        self.objects = objects

        # Create a tree structure based on the part of the name.
        self.tree = self._tree(objects)

        if len(objects) == 0:
            self._placeholders = 0
        elif objects[0].name_index[-1][1] == None:
            self._placeholders = max(0, len(objects[0].name_index) - 1)
        else:
            self._placeholders = len(objects[0].name_index)

        if len(objects) > 0:
            self.name = ''
            for i in range(0, len(objects[0].name_index)):
                ni = objects[0].name_index[i]
                if ni[1] == None:
                    self.name += ni[0]
                else:
                    self.name += f"{ni[0]}[{self.placeholders()[i]}]"
            self.cname = cname(self.name)
        else:
            self.cname = None
            self.name = None

    def placeholders(self):
        return 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'[:self._placeholders]

    def _tree(self, objects, index = 0):
        if len(objects) == 0:
            return None
        if len(objects) == 1:
            return objects[0]

        res = {}

        groups = {}
        # Group on subscript within this part of the name.
        for o in objects:
            subscript = o.name_index[index][1]
            if not subscript in groups:
                groups[subscript] = []
            groups[subscript].append(o)

        for subscript in groups.keys():
            res[subscript] = self._tree(groups[subscript], index + 1)

        return res

    def c_impl(self):
        return self._c_impl(self.tree, 0, self.placeholders(), '\t\t\t')

    def _c_impl(self, tree, index, placeholders, indent):
        if tree == None:
            return f'{indent}return Variant<Implementation>();\n'
        if not isinstance(tree, dict):
            return f'{indent}return this->{tree.cname}.variant();\n'

        res  = f'{indent}switch({placeholders[index]}) {{\n'
        for i in tree.keys():
            res += f'{indent}case {i}:\n'
            res += self._c_impl(tree[i], index + 1, placeholders, indent + '\t')
        res += f'{indent}default:\n{indent}\treturn Variant<Implementation>();\n'
        res += f'{indent}}}\n'
        return res

    def c_decl(self):
        return self.cname + '(' + ', '.join([f'int {x}' for x in self.placeholders()]) + ')'

class Store(object):
    def __init__(self, objects):
        self.objects = objects
        self.buffer = Buffer()
        self.directory = Directory()
        self.littleEndian = True
        self.hash = None

    def process(self):
        self.flattenScopes()
        self.checkNames()
        self.generateBuffer()
        self.generateDirectory()
        self.extractArrayAccessors()
        self.generateAxiAddresses()

    def flattenScope(self, scope):
        res = []
        for i in range(0, len(scope)):
            o = scope[i]
            if isinstance(o, Scope):
#                print(f'flatten {o.name}')
                flatten = self.flattenScope(self.expandArrays(o.objects))
                for f in flatten:
                    f.setName(o.name + '/' + f.name)
                res += flatten
            else:
                res.append(o)
        return res

    def flattenScopes(self):
        self.objects = self.expandArrays(self.objects)
        self.objects = self.flattenScope(self.objects)

    def copy(self, o):
        newo = copy.copy(o)
        if isinstance(newo, Scope):
            newo.objects = list(map(self.copy, newo.objects))
        return newo

    def expandArrays(self, objects):
        os = []
        for o in objects:
            if o.len > 1:
#                print(f'expand {o.name}')
                for i in range(0, o.len):
                    newo = self.copy(o)
                    newo.len = 1
                    newo.setName(newo.name + f'[{i}]')
                    os.append(newo)
            else:
                os.append(o)
        return os

    def checkNames(self):
        for o in self.objects:
            if len(list(filter(lambda x: x.name == o.name, self.objects))) > 1:
                sys.exit(f'Duplicate name "{o.name}"')
            if '//' in o.name:
                sys.exit(f'Empty scope name in "{o.name}"')

    def generateBuffer(self):
        initvars = []
        defaultvars = []
        for o in self.objects:
            if isinstance(o, Variable):
                assert o.len == 1
                if o.init == None:
                    defaultvars.append(o)
                else:
                    initvars.append(o)
            elif isinstance(o, Function):
                o.bump()

        initvars.sort(key=lambda o: o.size, reverse=True)
        defaultvars.sort(key=lambda o: o.size, reverse=True)
        self.buffer.generate(initvars, defaultvars, self.littleEndian)

    def generateDirectory(self):
        self.directory.generate(self.objects)

    def extractArrayAccessors(self):
        # Find all objects that look like having one or more arrays
        split_objects = []
        for o in self.objects:
            o.splitName()
            if o.name_index == None:
                continue
            split_objects.append(o)

        # Now, collect all similar objects.
        grouped_objects = {}
        for o in split_objects:
            key = []
            for i in range(0, len(o.name_index)):
                key.append(o.name_index[i][0])
            # Make hashable
            key = tuple(key)
            if not key in grouped_objects:
                grouped_objects[key] = []
            grouped_objects[key].append(o)

        # Save the grouped objects, which will later be used to generate nested switch statements.
        self.arrays = [ArrayLookup(x) for x in grouped_objects.values()]

    def generateAxiAddresses(self):
        addr = 0
        for o in self.objects:
            if isinstance(o, Variable):
                assert(o.len == 1)
                if o.size <= 4:
                    o.axi = addr
                    addr += 4


class Object(object):
    def __init__(self, parent, name, len = 0):
        self.setName(name)
        self.len = len if isinstance(len, int) and len > 1 else 1

    def setName(self, name):
        self.name = object_name(name)
        self.cname = cname(self.name)

    # Split our name to find common array indices across objects.
    def splitName(self):
        chunks = re.split(r'\[(\d+)\]', self.name)
        if len(chunks) == 1:
            # Nothing to split
            self.name_index = None
            return

        # chunks has now an alternating string/array index sequence.
        # Merge into pairs
        if chunks[-1] == '':
            # drop empty element at the end (the name ends with an array index)
            chunks.pop()
        if len(chunks) % 2 == 1:
            # odd number, must be ending in a non-array part of the name.
            # Add dummy index
            chunks.append(None)

        self.name_index = []
        for i in range(0, len(chunks), 2):
            self.name_index.append((chunks[i], chunks[i+1]))

class Variable(Object):
    def __init__(self, parent, type, name):
        super().__init__(self, name)
        self.parent = parent
        self.offset = 0
        if type.fixed != None:
            self.type = type.fixed.type
            self.size = csize(type.fixed.type)
            self.init = None if type.init is None or type.init.value == 0 else type.init.value
            self.len = type.fixed.len if isinstance(type.fixed.len, int) and type.fixed.len > 1 else 1
        else:
            self.type = type.blob.type
            self.size = type.blob.size
            self.init = None
            if self.type == 'string' and type.init is not None:
                self.init = bytes(str(type.init), "utf-8").decode("unicode_escape")
                l = len(self.init.encode())
                if self.size < l:
                    sys.exit(f'String initializer is too long')
                if l == 0:
                    # Empty string, handle as default-initialized.
                    self.init = None
            self.len = type.blob.len
        self.axi = None

    def isBlob(self):
        return self.type in ['blob', 'string']

    def _encode_string(self, x):
        s = x.encode()
        assert len(s) <= self.size
        return s + bytes([0] * (self.buffersize() - len(s)))

    def encode(self, x, littleEndian=True):
        endian = '<' if littleEndian else '>'
        res = {
                'bool': lambda x: struct.pack(endian + '?', not x in [False, 'false', 0]),
                'int8': lambda x: struct.pack(endian + 'b', int(x)),
                'uint8': lambda x: struct.pack(endian + 'B', int(x)),
                'int16': lambda x: struct.pack(endian + 'h', int(x)),
                'uint16': lambda x: struct.pack(endian + 'H', int(x)),
                'int32': lambda x: struct.pack(endian + 'i', int(x)),
                'uint32': lambda x: struct.pack(endian + 'I', int(x)),
                'int64': lambda x: struct.pack(endian + 'q', int(x)),
                'uint64': lambda x: struct.pack(endian + 'Q', int(x)),
                'float': lambda x: struct.pack(endian + 'f', float(x)),
                'double': lambda x: struct.pack(endian + 'd', float(x)),
                'ptr32': lambda x: struct.pack(endian + 'L', int(x)),
                'ptr64': lambda x: struct.pack(endian + 'Q', int(x)),
                'blob': lambda x: bytearray(x),
                'string': self._encode_string
        }[self.type](x)
        return res

    def buffersize(self):
        if self.type == 'string':
            return self.size + 1
        else:
            return self.size

    def __str__(self):
        return self.name

class Function(Object):
    f = 1

    def __init__(self, parent, type, name, len):
        super().__init__(self, name, len)
        self.parent = parent
        self.offset = self.f
        self.axi = None
        if type.fixed != None:
            self.type = type.fixed.type
            self.size = csize(type.fixed.type)
        else:
            self.type = type.blob.type
            self.size = type.blob.size

    def isBlob(self):
        return self.type in ['blob', 'string']

    def bump(self):
        self.f = Function.f
        Function.f += 1

    def __str__(self):
        return self.name

class Scope(Object):
    def __init__(self, parent, objects, name, len):
        super().__init__(self, name, len)
        self.parent = parent
        self.objects = objects

    def __str__(self):
        return self.name

class BlobType(object):
    def __init__(self, parent, type, size, len):
        self.parent = parent
        self.type = type
        self.len = len if len != None and len > 1 else 1
        self.size = size if size > 0 else 0

class StringType(BlobType):
    def __init__(self, parent, type, size, len):
        super().__init__(parent, type, size, len)

class Immediate(object):
    def __init__(self, parent, value):
        self.parent = parent
        if isinstance(value, str):
            if value.lower() == 'true':
                self.value = True
            elif value.lower() == 'false':
                self.value = False
            elif value.lower() == 'nan':
                self.value = float('nan')
            elif value.lower() in ['inf', 'infinity']:
                self.value = float('inf')
            elif value.lower() in ['-inf', '-infinity']:
                self.value = float('-inf')
            else:
                self.value = int(value, 0)
        else:
            self.value = value


