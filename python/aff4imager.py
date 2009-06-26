#!/usr/bin/python

from aff4 import *
import optparse
import sys

parser = optparse.OptionParser()
parser.add_option("-i", "--image", default=None,
                  action='store_true',
                  help = "Imaging mode")

parser.add_option("-o", "--output", default=None,
                  help="Create the output volume on this file or URL (using webdav)")

parser.add_option("", "--link", default="default",
                  help="Create a symbolic link to the image URN with this name")

parser.add_option("-D","--dump", default=None,
                  help="Dump out this stream (to --output or stdout)")

parser.add_option("-l","--load", default=[],
                  action = "append",
                  help="Load this volume to prepopulate the resolver")

parser.add_option("-I", "--info", default=None,
                  action = 'store_true',
                  help="Information mode - dump all information in the resolver")

parser.add_option("-k", "--key", default=None,
                  help="Key file to use (in PEM format)")

parser.add_option("-c", "--cert", default=None,
                  help="Certificate file to use (in PEM format)")

parser.add_option("-t", "--threads", default=2,
                  help="Number of threads to use")

(options, args) = parser.parse_args()

if not options.image and not options.dump and not options.info:
    print "You must specify a mode (try -h)"
    sys.exit(1)

oracle.set(GLOBAL, CONFIG_THREADS, options.threads)

VOLUMES = []
for v in options.load:
    VOLUMES.append(load_volume(v))

## Prepare an identity for signing
IDENTITY = Identity()
if options.key:
    IDENTITY.load_priv_key(options.key)

if options.cert:
    IDENTITY.load_certificate(options.cert)
IDENTITY.finish()

## Store the volume here:
output = options.output
if options.image:
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
        ## Make the link
        link_urn = fully_qualified_name(options.link, volume_urn)
        link = oracle.open(link_urn, 'r')
        if link:
            print "A link '%s' already exists in this volume. I wont create a link now - but you can add it later using the -L mode"
            oracle.cache_return(link)
        else:
            link = Link(link_urn)
            oracle.set(link.urn, AFF4_STORED, volume_urn)
            oracle.set(link.urn, AFF4_TARGET, image_urn)
            link.finish()
            link.close()
        
        oracle.cache_return(in_fd)

    ## Sign it if needed
    oracle.set(IDENTITY.urn, AFF4_STORED, volume_urn)
    IDENTITY.close()

    ## Close the volume off
    volume_fd = oracle.open(volume_urn, 'w')
    volume_fd.close()

elif options.dump:
    for v in VOLUMES:
        stream = oracle.open(fully_qualified_name(options.dump, v), 'r')
        if stream: break

    if output:
        output_fd = FileBackedObject(output, 'w')
    else:
        output_fd = sys.stdout
        
    try:
        while 1:
            data = stream.read(1024 * 1024)
            if not data: break

            output_fd.write(data)
    finally:
        oracle.cache_return(stream)
        if output:
            output_fd.close()
            
elif options.info:
    print oracle
