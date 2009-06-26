import sk
from aff4 import *
import optparse
import sys, os.path

parser = optparse.OptionParser()
parser.add_option("-l", "--load", default=None,
                  help = "Load the volume to prepopulate the resolver")

parser.add_option("-o", "--output", default=None,
                  help="Create the output volume on this file or URL (using webdav)")

parser.add_option("-s", "--stream", default="default",
                  help="Operate on this stream")

(options, args) = parser.parse_args()

VOLUME = ''

if options.load:
    VOLUME = load_volume(options.load)

if not options.output:
    print "You must specify an output volume to store the map files in"
    sys.exit(-1)

if "://" in options.output:
    output = options.output
else:
    output = "file://%s" % options.output
    
stream = oracle.open(fully_qualified_name(options.stream, VOLUME))

## Make an output volume FIXME - needs to be a utility
output_volume = ZipVolume(None, 'w')
oracle.set(output_volume.urn, AFF4_STORED, output)
output_volume.finish()
output_urn = output_volume.urn
oracle.cache_return(output_volume)

try:
    fs = sk.skfs(stream)
    block_size = fs.block_size
    for root, dirs, files in fs.walk('/', unalloc=True, inodes=True):
        for f, filename in files:
            print root[1], filename
            pathname = os.path.join(root[1], filename)

            ## The maps have urns based on the path names in the filesystem:
            map_stream = Map(None, 'w')
            map_stream.urn = fully_qualified_name(pathname, output_urn)
            
            s = fs.stat(inode = str(f))

            ## Some of these properties should be stored
            oracle.set(map_stream.urn, AFF4_SIZE, s.st_size)
            oracle.set(map_stream.urn, AFF4_STORED, output_urn)
            oracle.set(map_stream.urn, AFF4_TARGET, stream.urn)
            map_stream.finish()
            
            ## Now list the blocks
            fd = fs.open(inode = str(f))
            blocks = fd.blocks()
            block = 0
            for b in blocks:
                map_stream.add(block * block_size, b * block_size, stream.urn)
                block += 1

            ## Now save the map stream
            map_stream.close()
finally:
    oracle.cache_return(stream)

    ## Make sure we close off the volume properly
    volume = oracle.open(output_urn, 'w')
    volume.close()
