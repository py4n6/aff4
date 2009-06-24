#!/usr/bin/python

from aff4 import *
import optparse

parser = optparse.OptionParser()
parser.add_option("-i", "--image", default=None,
                  action='store_true',
                  help = "Imaging mode")

parser.add_option("-o", "--output", default=None,
                  help="Create the output volume on this file or URL (using webdav)")

(options, args) = parser.parse_args()

if not options.image:
    print "You must specify a mode (try -h)"
    sys.exit(1)

if options.image:
    ## Store the volume here:
    output = options.output
    if "://" not in output:
        output = "file://" + output

    ## Make a new volume
    volume_fd = ZipVolume(None, "w")
    volume_urn = volume_fd.urn
    oracle.add(volume_urn, AFF4_STORED, output)
    volume_fd.finish()
    oracle.cache_return(volume_fd)

    for image in args:
        ## Make an image
        image_fd = Image(None, "w")
        oracle.add(image_fd.urn, AFF4_STORED, volume_urn)
        image_urn = image_fd.urn
        image_fd.finish()

        ## Make sure its a fully qualified url
        if "://" not in image:
            image = "file://%s" % image
            
        in_fd = oracle.open(image)

        while 1:
            data = in_fd.read(1024 * 1024)
            if not data: break

            image_fd.write(data)

        image_fd.close()
        oracle.cache_return(in_fd)
    
    volume_fd = oracle.open(volume_urn, 'w')
    volume_fd.close()
