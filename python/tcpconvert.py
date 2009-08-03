from optparse import OptionParser
import sys
try:
    import reassembler
except ImportError:
    print """You need to have PyFlag installed from http://www.pyflag.net/ for this to work. The reassembler module must also be on the python class path (do you need to run pyflag_launch to set the class path?)"""
    sys.exit(1)
    
import pypcap, pdb
from aff4 import *

parser = OptionParser(usage = """%prog [options] -o output.aff4 pcap_file ... pcap_file

Will merge pcap files into the output aff4 volume (which will be created if needed) while reassembling them. Streams will be extracted into aff4 map streams and added to the volume.
""")

parser.add_option("-o", "--output", default=None,
                  help="Create the output volume on this file or URL (using webdav)")

parser.add_option("-e", "--external", default=False,
                  action='store_true',
                  help="Do not create a pcap stream, instead refer directly to external pcap file, and only create map files.")

parser.add_option("-l","--load", default=[],
                  action = "append",
                  help="Load this volume to prepopulate the resolver")

parser.add_option("-v", "--verbosity", default=5, type='int',
                  help = "Level of verbosity")

parser.add_option("-k", "--key", default=None,
                  help="Key file to use (in PEM format)")

parser.add_option("-c", "--cert", default=None,
                  help="Certificate file to use (in PEM format)")

(options, args) = parser.parse_args()

VOLUMES = []
for v in options.load:
    VOLUMES.extend(load_volume(v))

oracle.set(GLOBAL, CONFIG_VERBOSE, options.verbosity)

## Prepare an identity for signing
IDENTITY = load_identity(options.key, options.cert)

## Store the volume here:
output = options.output
if not options.external:
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
        try:
            ## Make sure its a fully qualified url
            if "://" not in image:
                image = "file://%s" % image
            
            in_fd = oracle.open(image)
            pcap_file = pypcap.PyPCAP(in_fd)
        except IOError:
            print "%s does not appear to be a pcap file" % image
            continue
        
        ## Make an image
        image_fd = Image(None, "w")
        oracle.add(image_fd.urn, AFF4_STORED, volume_urn)
        image_urn = image_fd.urn
        image_fd.finish()

        ## Write a pcap header on:
        image_fd.write(pcap_file.file_header().serialise())

        def Callback(mode, packet, connection):
            if mode == 'est':
                ## Now do the reverse connection
                if 'map' not in connection:
                    ip = packet.find_type("IP")
                    ## We can only get tcp or udp packets here
                    try:
                        tcp = packet.find_type("TCP")
                    except AttributeError:
                        tcp = packet.find_type("UDP")

                    base_urn = "%s/%s-%s/%s-%s/" % (
                        volume_urn,
                        ip.source_addr, ip.dest_addr,
                        tcp.source, tcp.dest)

                    combined_stream = Map(None, 'w')
                    connection['reverse']['combined'] = combined_stream
                    connection['combined'] = combined_stream
                    combined_stream.urn = base_urn + "combined"
                    
                    oracle.set(combined_stream.urn, AFF4_STORED, volume_urn)
                    oracle.set(combined_stream.urn, AFF4_TARGET, image_urn)
                    oracle.set(combined_stream.urn, AFF4_TIMESTAMP, packet.ts_sec)
                    combined_stream.finish()
                    
                    map_stream = connection['map'] = Map(None, 'w')
                    map_stream.urn = base_urn + "forward"
                    
                    oracle.set(map_stream.urn, AFF4_STORED, volume_urn)
                    oracle.set(map_stream.urn, AFF4_TARGET, image_urn)
                    oracle.set(map_stream.urn, AFF4_TIMESTAMP, packet.ts_sec)
                    map_stream.finish()

                    connection['reverse']['map'] = map_stream = Map(None, 'w')
                    map_stream.urn = base_urn + "reverse"

                    oracle.set(map_stream.urn, AFF4_STORED, volume_urn)
                    oracle.set(map_stream.urn, AFF4_TARGET, image_urn)
                    oracle.set(map_stream.urn, AFF4_TIMESTAMP, packet.ts_sec)
                    map_stream.finish()

            elif mode == 'data':
                try:
                    tcp = packet.find_type("TCP")
                except AttributeError:
                    tcp = packet.find_type("UDP")

                length = len(tcp.data)
                connection['map'].write_from("@", packet.offset + tcp.data_offset, length)
                connection['combined'].write_from("@", packet.offset + tcp.data_offset,
                                                  length)

            elif mode == 'destroy':
                if connection['map'].size > 0 or connection['reverse']['map'].size > 0:
                    map_stream = connection['map']
                    map_stream.close()

                    r_map_stream = connection['reverse']['map']
                    r_map_stream.close()

                    combined_stream = connection['combined']
                    combined_stream.close()

        ## Create a tcp reassembler
        processor = reassembler.Reassembler(packet_callback = Callback)

        while 1:
            try:
                packet = pcap_file.dissect()
                processor.process(packet)
                image_fd.write(packet.serialise())
            except StopIteration: break

        ## Make sure all map streams are written by forcing the processor to be freed
        del processor
        image_fd.close()

    ## Sign it if needed
    if IDENTITY:
        oracle.set(IDENTITY.urn, AFF4_STORED, volume_urn)
        IDENTITY.close()

    ## Close the volume off
    volume_fd = oracle.open(volume_urn, 'w')
    volume_fd.close()
