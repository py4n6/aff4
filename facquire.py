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

parser.add_option("-c", "--chunksize", default=32,
                  type='int',
                  help="Block size in kb")

parser.add_option("-S", "--volume_size", default=650,
                  type='int',
                  help='Maximum volume size in MB')

parser.add_option("-e", "--encrypt", default=False,
                  action='store_true',
                  help='Should we encrypt the output file')

parser.add_option("-E", "--scheme", default='aes-sha-psk',
                  help='Default encryption scheme')

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
basefif = fiffile = fif.FIFFile()

## Create a new volume
count = 0
new_name = "%s.%02d.zip" % (args[1], count)
print "Creating new volume %s" % new_name
basefif.create_new_volume(new_name)

## Make a new stream on the new volume if needed. Encryption means to
## create a Encrypted stream on basefif. We create a new FIFFile and
## tell it to make a new volume on the encrypted stream.
if options.encrypt:
    enc = basefif.create_stream_for_writing(stream_name='crypted',
                                            stream_type='Encrypted',
                                            scheme = options.scheme,
                                            )

    fiffile = enc.create_new_volume()

## Now we create a new Image stream on the fiffile to store the image
## in:
stream = fiffile.create_stream_for_writing(stream_name=options.stream,
                                           stream_type = "Image",
                                           chunksize = options.chunksize * 1024)

## This is the maxium volume size - we break up the volume when we
## exceed it:
max_size = options.volume_size * 1024 * 1024
while 1:
    if basefif.size > max_size:
        count += 1
        new_name = "%s.%02d.zip" % (args[1], count)
        print "Creating new volume %s" % new_name
        ## Make sure that the old volume knows about the new one:
        basefif.properties['volume'] = "file://" + new_name

        basefif.close()
        basefif.create_new_volume(new_name)
        
    data = infd.read(1024 * 1024)
    if len(data)==0:
        break

    stream.write(data)

## Close off all the streams in this order
stream.close()
fiffile.close()
if options.encrypt:
    enc.close()
    
basefif.close()
