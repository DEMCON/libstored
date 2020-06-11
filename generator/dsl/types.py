# vim:et
import re
import struct

cnames = {}

def cname(s):
    if s in cnames:
        return cnames[s]
    c = s
    c = re.sub(r'[^A-Za-z0-9/]+', '_', c)
    c = re.sub(r'/+', '__', c)
    c = re.sub(r'^[^A-Za-z]', '_', c)
    c = re.sub(r'_+$', '', c)

    u = c
    i = 2
    while u in cnames.values():
        u = c + f'_{i}'
        i += 1

    cnames[s] = u
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

def object_name(s):
    s = re.sub(r'\s+', ' ', s)
    return s

class Directory(object):
    def __init__(self):
        self.data = [1,2,3]

class Buffer(object):
    def __init__(self):
        self.size = 10
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

    def generate(self, initvars, defaultvars):
        self.init = []
        for v in initvars:
            size = v.buffersize()
            alignedsize = self.align(size)
            v.offset = len(self.init)
            self.init += list(v.encode(v.default))
            if alignedsize > size:
                self.init += [0] * (alignedsize - size)

        self.size = self.align(len(self.init), 8)
        for v in defaultvars:
            size = v.buffersize()
            alignedsize = self.align(size)
            v.offset = self.size
            self.size += size

class Store(object):
    def __init__(self, objects):
        self.objects = objects
        self.buffer = Buffer()
        self.directory = Directory()

    def generateBuffer(self):
        initvars = []
        defaultvars = []
        for o in self.objects:
            if isinstance(o, Variable):
                if o.default == None:
                    defaultvars.append(o)
                else:
                    initvars.append(o)

        initvars.sort(key=lambda o: o.size, reverse=True)
        defaultvars.sort(key=lambda o: o.size, reverse=True)
        self.buffer.generate(initvars, defaultvars)

class Variable(object):
    def __init__(self, parent, type, name, default):
        self.parent = parent
        self.type = type
        self.name = object_name(name)
        self.cname = cname(self.name)
        self.default = default
        self.offset = 0
        self.size = type.size if isinstance(type, BlobType) else csize(self.type)

    def encode(self, x, littleEndian=True):
        endian = '<' if littleEndian else '>'
        res = {
                'bool': lambda x: struct.pack(endian + '?', x),
                'int8': lambda x: struct.pack(endian + 'b', x),
                'uint8': lambda x: struct.pack(endian + 'B', x),
                'int16': lambda x: struct.pack(endian + 'h', x),
                'uint16': lambda x: struct.pack(endian + 'H', x),
                'int32': lambda x: struct.pack(endian + 'i', x),
                'uint32': lambda x: struct.pack(endian + 'I', x),
                'int64': lambda x: struct.pack(endian + 'q', x),
                'uint64': lambda x: struct.pack(endian + 'Q', x),
                'float': lambda x: struct.pack(endian + 'f', x),
                'double': lambda x: struct.pack(endian + 'd', x),
                'ptr32': lambda x: struct.pack(endian + 'L', x),
                'ptr64': lambda x: struct.pack(endian + 'Q', x),
                'blob': lambda x: bytearray(x),
                'string': lambda x: x.encode(),
        }[self.type](x)
        return res

    def buffersize(self):
        if self.type == 'string':
            return self.size + 1
        else:
            return self.size

    def __str__(self):
        return self.name

class Function(object):
    f = 1

    def __init__(self, parent, type, name):
        self.parent = parent
        self.type = type
        self.name = object_name(name)
        self.cname = cname(self.name)
        self.f = Function.f
        self.offset = self.f
        self.size = 0
        Function.f += 1
    
    def __str__(self):
        return self.name

class BlobType(object):
    def __init__(self, parent, type, size):
        self.parent = parent
        self.type = type
        self.size = size

