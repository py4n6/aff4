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

parser.add_option("-S", "--volume_size", default=650,
                  type='int',
                  help='Maximum volume size in MB')

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

## Get a handle to the FIF file
fiffile = fif.FIFFile()

## Create a new volume
count = 0
new_name = "%s.%02d.zip" % (args[1], count)
fiffile.create_new_volume(new_name)

## Make a new stream on the new volume
stream = fiffile.create_stream_for_writing(stream_name=options.stream)

max_size = options.volume_size * 1024 * 1024
while 1:
    if fiffile.size > max_size:
        count += 1
        new_name = "%s.%02d.zip" % (args[1], count)
        print "Creating new volume %s" % new_name
        stream.write_properties(fiffile)
        fiffile.create_new_volume(new_name)
        
    data = infd.read(options.blocksize * 1024)
    if len(data)==0:
        break
    
    stream.write_chunk(data)
    
stream.close()
