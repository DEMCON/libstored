# vim:et
import re

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

def object_name(s):
    s = re.sub(r'\s+', ' ', s)
    return s

class Store(object):
    def __init__(self, objects):
        self.objects = objects

class Variable(object):
    def __init__(self, parent, type, name, default):
        self.parent = parent
        self.type = type
        self.name = object_name(name)
        self.cname = cname(self.name)
        self.default = default
        self.offset = 0
        self.size = type.size if isinstance(type, BlobType) else 0

    def __str__(self):
        return self.name

class Function(object):
    f = 1

    def __init__(self, parent, type, name):
        self.parent = parent
        self.type = type
        self.name = object_name(name)
        self.cname = cname(self.name)
        self.offset = 0
        self.f = Function.f
        Function.f += 1
        self.size = type.size if isinstance(type, BlobType) else 0
    
    def __str__(self):
        return self.name

class BlobType(object):
    def __init__(self, parent, type, size):
        self.parent = parent
        self.type = type
        self.size = size

