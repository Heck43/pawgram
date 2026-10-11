"""
exteraGram Desktop SDK - Hook Utilities
"""

from java import JavaClassStub, jclass

def find_class(class_name: str):
    return jclass(class_name)

def get_private_field(obj, field_name: str):
    return getattr(obj, field_name, None)

def set_private_field(obj, field_name: str, value):
    try:
        setattr(obj, field_name, value)
    except Exception:
        pass
