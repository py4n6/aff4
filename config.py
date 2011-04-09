""" This file contains configuration information for the AFF4 build system.

It will be imported by the build system. This is a regular python file
so it must be python syntax.
"""
import os

## This is a path to the location of persistent TDB files.
TDB_LOCATION = "/tmp/"

## Debug level. V=0 means no debugging and full optimization enabled.
V = True

## Optional build modules
disable_ewf = True
disable_aff = True
disable_curl = True

## This can be any combination of ('lock','resolver','object')
DEBUG = []

## This enables the external interprocess locks. If it is disabled,
## the multiple process access to the same resolver will result in
## corruption.
PROCESS_LOCKS = False

## This is where the installation lives
PREFIX = "/home/scudette/prefix/"

## Location of the libxml2 headers - If this is None we call xml2-config
LIBXML2_INCLUDES = None
LIBXML2_LINK = None

## Location of libxslt header
LIBXSLT_INCLUDES = None
LIBXSLT_LINK = None

## Should we build these optional components of libraptor
RAPTOR_TURTLE = True
RAPTOR_NTRIPLES = True
RAPTOR_RDFXML = True

## Location of tsk3 headers
TSK3_HEADER_LOCATION = "/usr/include/tsk3/"

## Location of regfi headers
REGFI_HEADER_LOCATION = "/usr/local/include/regfi/"

## Darwin specific options (Only used when running on OSX)
DARWIN_ARCHITECTURE = "-arch x86_64"

MINGW_XCOMPILE = False
XCOMPILE_PYTHON_PATH = "%s/.wine/drive_c/Python26/include/" % os.environ['HOME']
