"""
exteraGram Desktop SDK - Java Bridge & Android Ecosystem Emulation
Provides stubs and emulation for Java/Android classes so mobile plugins can load seamlessly on Desktop.
"""

import sys
import types
import os

class JavaClassStub:
    def __init__(self, name: str = ""):
        self._name = name

    def __getattr__(self, item):
        return JavaClassStub(f"{self._name}.{item}")

    def __call__(self, *args, **kwargs):
        return JavaClassStub(self._name)

    def __str__(self):
        return f"<JavaClass {self._name}>"

    def __repr__(self):
        return f"<JavaClass {self._name}>"

    def getClass(self):
        return self

    def getDeclaredMethod(self, name, *args):
        return JavaMethodStub(name)

    def getMethod(self, name, *args):
        return JavaMethodStub(name)

    def getDeclaredMethods(self):
        return []

    def getMethods(self):
        return []

    def getField(self, name):
        return JavaFieldStub(name)

    def getDeclaredField(self, name):
        return JavaFieldStub(name)

    def getDeclaredFields(self):
        return []

    def getFields(self):
        return []

    TYPE = "TYPE"

class JavaMethodStub:
    def __init__(self, name: str):
        self._name = name

    def getName(self):
        return self._name

    def setAccessible(self, flag: bool):
        pass

    def invoke(self, obj, *args):
        return None

    def __call__(self, *args, **kwargs):
        return None

class JavaFieldStub:
    def __init__(self, name: str):
        self._name = name

    def getName(self):
        return self._name

    def setAccessible(self, flag: bool):
        pass

    def get(self, obj):
        return None

    def set(self, obj, val):
        pass

class JavaArrayStub:
    @staticmethod
    def newInstance(cls, length):
        return JavaClassStub("ArrayInstance")

def jclass(name: str):
    return JavaClassStub(name)

def dynamic_proxy(interface_class):
    return object

# Java Standard Collections & Types
class Locale:
    @staticmethod
    def getDefault():
        class _Def:
            @staticmethod
            def getLanguage():
                return "ru"
        return _Def()

class ArrayList(list):
    def toArray(self):
        return list(self)

    def add(self, item):
        super().append(item)

    def get(self, index):
        return self[index]

    def size(self):
        return len(self)

class Arrays:
    @staticmethod
    def asList(values):
        if hasattr(values, "__iter__"):
            return list(values)
        return [values]

class String(str): pass
class Integer(int): TYPE = int
class Boolean: TYPE = bool
class Long(int): TYPE = int
class Object(object): pass
class Runnable(object): pass
class CharSequence(str): pass
class Class(object): pass

class _DynamicLoader:
    def __init__(self, module):
        self.module = module

    def create_module(self, spec):
        return self.module

    def exec_module(self, module):
        pass

# Dynamic Module Importer for android.*, org.*, com.*, de.*, java.*
class _DynamicStubModule(types.ModuleType):
    def __init__(self, fullname):
        super().__init__(fullname)
        self.__file__ = f"<virtual {fullname}>"
        self.__package__ = fullname
        self.__path__ = []

    def __getattr__(self, name):
        if name == "Locale":
            return Locale
        if name == "ArrayList":
            return ArrayList
        if name == "Arrays":
            return Arrays
        if name == "Array":
            return JavaArrayStub
        if name in ("Integer", "Boolean", "Long", "String", "Object", "Runnable", "CharSequence", "Class"):
            return globals().get(name, JavaClassStub(name))
        stub = JavaClassStub(f"{self.__name__}.{name}")
        setattr(self, name, stub)
        return stub

class _DynamicStubFinder:
    ROOTS = ("java", "android", "org", "com", "de")

    def find_spec(self, fullname, path, target=None):
        parts = fullname.split(".")
        if parts[0] in self.ROOTS:
            from importlib.machinery import ModuleSpec
            mod = _DynamicStubModule(fullname)
            sys.modules[fullname] = mod
            spec = ModuleSpec(fullname, _DynamicLoader(mod), origin="dynamic_stub", is_package=True)
            spec.submodule_search_locations = []
            return spec
        return None

def install_java_importer():
    for f in sys.meta_path:
        if isinstance(f, _DynamicStubFinder):
            return
    sys.meta_path.insert(0, _DynamicStubFinder())

install_java_importer()
