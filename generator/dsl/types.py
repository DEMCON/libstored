# vim:et
import re
import struct
import copy

cnames = {}

def cname(s):
    if s in cnames:
        return cnames[s]
    c = s
    c = re.sub(r'[^A-Za-z0-9/]+', '_', c)
    c = re.sub(r'/+', '__', c)
    c = re.sub(r'^__', '', c)
    c = re.sub(r'^[^A-Za-z]_*', '_', c)
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
        if len(h) == 1:
            if '/' in h:
                self.stripUnambig(h['/'])
                return
            elif '\x00' in h:
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
        self.longdata = [ord('/')] + self.generateDict(h) + [0]
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
            self.size += size

class Store(object):
    def __init__(self, objects):
        self.objects = objects
        self.buffer = Buffer()
        self.directory = Directory()
        self.littleEndian = True

    def process(self):
        self.flattenScopes()
        self.expandArrays()
        self.generateBuffer()
        self.generateDirectory()
    
    def flattenScope(self, scope):
        res = []
        for i in range(0, len(scope)):
            o = scope[i]
            if isinstance(o, Scope):
                flatten = self.flattenScope(o.objects)
                for f in flatten:
                    f.setName(o.name + '/' + f.name)
                res += flatten
            else:
                res.append(o)
        return res

    def flattenScopes(self):
        self.objects = self.flattenScope(self.objects)

    def expandArrays(self):
        os = []
        while self.objects != []:
            o = self.objects.pop(0)
            if o.len > 1:
                for i in range(0,o.len):
                    newo = copy.copy(o)
                    newo.len = 1
                    newo.setName(newo.name + f'[{i}]')
                    os.append(newo)
                    if isinstance(o, Function):
                        o.bump()
            else:
                os.append(o)
        self.objects = os

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

        initvars.sort(key=lambda o: o.size, reverse=True)
        defaultvars.sort(key=lambda o: o.size, reverse=True)
        self.buffer.generate(initvars, defaultvars, self.littleEndian)

    def generateDirectory(self):
        self.directory.generate(self.objects)

class Object(object):
    def __init__(self, parent, name, len = 0):
        self.setName(name)
        self.len = len if len > 1 else 1
    
    def setName(self, name):
        self.name = object_name(name)
        self.cname = cname(self.name)

class Variable(Object):
    def __init__(self, parent, type, name):
        super().__init__(self, name)
        self.parent = parent
        self.offset = 0
        if type.fixed != None:
            self.type = type.fixed.type
            self.size = csize(type.fixed.type)
            self.init = type.init
            self.len = type.fixed.len if type.fixed.len > 1 else 1
        else:
            self.type = type.blob.type
            self.size = type.blob.size
            self.init = None
            self.len = 1

    def isBlob(self):
        return self.type in ['blob', 'string']

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

class Function(Object):
    f = 1

    def __init__(self, parent, type, name, len):
        super().__init__(self, name)
        self.parent = parent
        self.offset = self.f
        self.size = 0
        self.len = len if len > 1 else 1
        if type.fixed != None:
            self.type = type.fixed.type
        else:
            self.type = type.blob.type
        self.bump()
    
    def isBlob(self):
        return self.type in ['blob', 'string']
    
    def bump(self):
        self.f = Function.f
        Function.f += 1

    def __str__(self):
        return self.name

class Scope(Object):
    def __init__(self, parent, objects, name):
        super().__init__(self, name)
        self.parent = parent
        self.objects = objects

    def __str__(self):
        return self.name

class BlobType(object):
    def __init__(self, parent, type, size):
        self.parent = parent
        self.type = type
        self.size = size

