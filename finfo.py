""" This program displays detailed information about FIF filesets.

We can also verify signatures and test the fileset integrity.
"""
import fif, optparse, sys, os

parser = optparse.OptionParser()
(options, args) = parser.parse_args()

if len(args)==0:
    print "You must specify a source and a destination"
    sys.exit(1)

## Open all the args as a fif fileset
fiffile = fif.FIFFile(args)
for f in fiffile.file_offsets.keys():
    print f
