""" The Forensic Interchange Format (FIF) is an extensible format for
forensic acquisition and interchange.
"""

import fif, optparse, sys, os

parser = optparse.OptionParser()

(options, args) = parser.parse_args()

if len(args)!=1:
    print "You must specify a FIF file"
    sys.exit(1)


## Get a handle to the FIF file
basefif = fif.FIFFile([args[0]], None, False, False)

print basefif.info.pretty_print()

#print basefif.info

basefif.close()
