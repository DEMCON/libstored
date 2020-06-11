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
    c = re.sub(r'^__', '', c)
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

def typeflags(s, func=False):
    return {
            'bool': 0x21,
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
        while i > 0:
            if i >= 0x80:
                res += [(i & 0x7f) | 0x80]
            else:
                res += [i]
            i = i >> 7

        return res

    def encodeType(self, o):
        func = isinstance(o, Function)
        if isinstance(o.type, BlobType):
            return [typeflags(o.type.type, func) + 0x80] + self.encodeInt(o.type.size)
        else:
            return [typeflags(o.type, func) + 0x80]

    def generateDict(self, h):
        if isinstance(h, Variable):
            return self.encodeType(h) + self.encodeInt(h.offset)
        elif isinstance(h, Function):
            return self.encodeType(h) + self.encodeInt(h.f)
        # else: must be a dict

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
        else:
            # Choose pivot to compare to
            pivot = int(len(names) / 2)
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
        if len(h) == 1 and ('/' in h or '\x00' in h):
            return

#        print(f'strip {h}')
        for k in list(h.keys()):
            v = h[k]
            if not isinstance(v, dict):
                continue
            self.stripUnambig(v)
            if len(v) == 1:
                vk = list(v.keys())[0]
                if vk == '/' or vk == '\x00':
                    continue
                vv = v[vk]
                if len(vv) == 1:
                    vvk = list(vv.keys())[0]
                    if vvk == '/' or vvk == '\x00':
#                        print(f'drop {vk}')
                        h[k] = vv
#        print(f'stripped {h}')

    def generate(self, objects):
        h = self.hierarchical(objects)
        self.stripUnambig(h)
        self.data = [ord('/')] + self.generateDict(h) + [0]
#        print(self.data)

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

    def generateDirectory(self):
        self.directory.generate(self.objects)

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

