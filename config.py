""" This file contains configuration information for the AFF4 build system.

It will be imported by the build system. This is a regular python file
so it must be python syntax.
"""
## This is a path to the location of persistent TDB files.
TDB_LOCATION = "/tmp/"

## Debug level. V=0 means no debugging and full optimization enabled.
V = True

## Optional build modules
disable_ewf = False
disable_aff = False
disable_curl = True

## This can be any combination of ('lock','resolver','object')
DEBUG = []

## This enables the external interprocess locks. If it is disabled,
## the multiple process access to the same resolver will result in
## corruption.
PROCESS_LOCKS = False

## This is where the installation leaves
PREFIX = "/usr/local/"

