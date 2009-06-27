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

parser.add_option("-V", "--verify", default=None,
                  action = 'store_true',
                  help="Verify all signatures and hashes when in --info mode")

parser.add_option("-k", "--key", default=None,
                  help="Key file to use (in PEM format)")

parser.add_option("-c", "--cert", default=None,
                  help="Certificate file to use (in PEM format)")

parser.add_option("-t", "--threads", default=2,
                  help="Number of threads to use")

parser.add_option('-v', '--verbosity', default=5,
                  help="Verbosity")

(options, args) = parser.parse_args()

## Load defaults into configuration space
oracle.set(GLOBAL, CONFIG_THREADS, options.threads)
oracle.set(GLOBAL, CONFIG_VERBOSE, options.verbosity)

VOLUMES = []
for v in options.load:
    VOLUMES.append(load_volume(v))

## Prepare an identity for signing
IDENTITY = None
if options.key:
    try:
        IDENTITY = Identity()
        IDENTITY.load_priv_key(options.key)
        
        if options.cert:
            IDENTITY.load_certificate(options.cert)
        else:
            raise RuntimeError("You must provide the certificate as well for signing")
        
        IDENTITY.finish()
    except RuntimeError,e:
        print e
    
## Store the volume here:
output = options.output
if options.image:
    ## Make a new volume
    volume_fd = ZipVolume(None, "w")
    try:
        volume_fd.load_from(output)
    except: pass
    
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
    if IDENTITY:
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
    
elif options.verify:
    ## Scan all volumes for identities
    for volume_urn in VOLUMES:
        for identity_urn in oracle.resolve_list(volume_urn, AFF4_IDENTITY_STORED):
            ## Open each identity and verify it
            identity = oracle.open(identity_urn, 'r')
            try:
                print "\n****** Identity %s verifies *****" % identity_urn
                print "    CN: %s \n" % identity.x509.get_subject().as_text()
                def print_result(uri, attribute, value, calculated):
                    if value == calculated:
                        print "OK  %s (%s)" % (uri, value.encode("hex"))
                    else:
                        print "WRONG HASH DETECTED in %s (found %s, should be %s)" % \
                              (uri, calculated.encode("hex"), value.encode("hex"))

                identity.verify(verify_cb = print_result)
            finally: oracle.cache_return(identity)
else:
    print "You must specify a mode (try -h)"
