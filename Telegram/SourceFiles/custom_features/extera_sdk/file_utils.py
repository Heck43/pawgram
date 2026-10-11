"""
exteraGram Desktop SDK - File Utilities
"""

import os
import sys

def get_plugins_dir() -> str:
    exe_dir = os.path.dirname(sys.executable)
    return os.path.join(exe_dir, "extera_plugins")

def get_files_dir() -> str:
    exe_dir = os.path.dirname(sys.executable)
    return os.path.join(exe_dir, "extera_files")
