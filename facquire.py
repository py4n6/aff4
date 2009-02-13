""" The Forensic Interchange Format (FIF) is an extensible format for
forensic acquisition and interchange.
"""

import fif, optparse, sys, os

parser = optparse.OptionParser()
parser.add_option("-s", "--stream", default="data",
                  help = "Stream name to add to FIF file")

parser.add_option("-a", "--append", default=False,
                  action='store_true',
                  help="Specify this if you want to append to an existing file")

parser.add_option("-b", "--blocksize", default=32,
                  type='int',
                  help="Block size in kb")

(options, args) = parser.parse_args()

if len(args)!=2:
    print "You must specify a source and a destination"
    sys.exit(1)

if os.access(args[1], os.F_OK) and not options.append:
    print "%s already exists - you must specify -a to append to an existing file" % args[1]
    sys.exit(1)

infd = open(args[0],'r')
mode = 'w'
if options.append: mode='a'

stream = fif.FIFWriter(args[1], mode, stream_name=options.stream)

while 1:
    data = infd.read(options.blocksize * 1024)
    if len(data)==0:
        break
    
    stream.write_chunk(data)
    
stream.finish()
