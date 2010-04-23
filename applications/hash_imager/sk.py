""" A Ctypes interface for sleuthkit """
from ctypes import *
import ctypes.util
import pdb

possible_names = ['libtsk3', 'tsk3',]
for name in possible_names:
    resolved = ctypes.util.find_library(name)
    if resolved:
        break

try:
    if resolved == None:
        raise ImportError("libtsk not found")
    libtsk = CDLL(resolved)
    if not libtsk._name: raise OSError()
except OSError:
    raise ImportError("libtsk not found")

a = libtsk.tsk_img_open(1, byref(c_char_p("/etc/passwd")), 0, 0)
pdb.set_trace()
