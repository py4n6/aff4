import pypcap, reassembler
import time
import pyaff4, os
time.sleep(1)

oracle = pyaff4.Resolver()

image_urn = pyaff4.RDFURN()
image_urn.set("/var/tmp/uploads/testimages/stdcapture_0.4.pcap")

image = oracle.open(image_urn, 'r')
pcap_file = pypcap.PyPCAP(image)

## Create a new volume on the output file
outfile = pyaff4.RDFURN()
outfile.set("/tmp/output.aff4")

try:
    os.unlink(outfile.parser.query)
except: pass

volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
volume.set(pyaff4.AFF4_STORED, outfile)
volume = volume.finish()
volume_urn = volume.urn
oracle.cache_return(volume)

def Callback(mode, packet, connection):

    if packet:
        try:
            ip = packet.find_type("IP")
        except AttributeError:
            ip = None

        try:
            tcp = packet.find_type("TCP")
        except AttributeError:
            try:
                tcp = packet.find_type("UDP")
            except AttributeError:
                tcp = None

    if mode == 'est':
        ## Now do the reverse connection
        if 'map' not in connection:
            ## Create the forward stream
            forward_stream = oracle.create(pyaff4.AFF4_MAP)
            forward_stream.urn.set(volume_urn.value)
            forward_stream.urn.add("%s-%s" % (ip.source_addr, ip.dest_addr))
            forward_stream.urn.add("%s-%s" % (tcp.source, tcp.dest))
            forward_stream.urn.add("forward")
            forward_stream.set_data_type(pyaff4.AFF4_MAP_BINARY)

            forward_stream.set(pyaff4.AFF4_STORED, volume_urn)

            timestamp = pyaff4.XSDDatetime()
            timestamp.set(packet.ts_sec)
            forward_stream.set(pyaff4.AFF4_TIMESTAMP, timestamp)

            forward_stream = forward_stream.finish()

            ## Note that we hold the map locked while its in the
            ## reassembler - this prevents it from getting freed
            connection['map'] = forward_stream

            ## Make the reverse map
            reverse_stream = oracle.create(pyaff4.AFF4_MAP)
            reverse_stream.urn.set(volume_urn.value)
            reverse_stream.urn.add("%s-%s" % (ip.source_addr, ip.dest_addr))
            reverse_stream.urn.add("%s-%s" % (tcp.source, tcp.dest))
            reverse_stream.urn.add("reverse")
            reverse_stream.set_data_type(pyaff4.AFF4_MAP_BINARY)

            reverse_stream.set(pyaff4.AFF4_STORED, volume_urn)

            timestamp = pyaff4.XSDDatetime()
            timestamp.set(packet.ts_sec)
            reverse_stream.set(pyaff4.AFF4_TIMESTAMP, timestamp)

            reverse_stream = reverse_stream.finish()

            ## Note that we hold the map locked while its in the
            ## reassembler - this prevents it from getting freed
            connection['reverse']['map'] = reverse_stream

    elif mode == 'data':
        length = len(tcp.data)
        connection['map'].write_from(image_urn, packet.offset + tcp.data_offset, length)

    elif mode == 'destroy':
        if connection['map'].size > 0 or connection['reverse']['map'].size > 0:
            map_stream = connection['map']
            map_stream.close()

            r_map_stream = connection['reverse']['map']
            r_map_stream.close()

processor = reassembler.Reassembler(packet_callback = Callback)

while 1:
    try:
        packet = pcap_file.dissect()
        processor.process(packet)
    except StopIteration: break

## This flushes the resolver which ensures all the 
del processor

volume = oracle.open(volume_urn, 'w')
volume.close()

stream_urn = pyaff4.RDFURN()
stream_urn.set(volume_urn.value)
stream_urn.add("192.168.0.2-203.134.152.45/40290-110/reverse")

fd = oracle.open(stream_urn, 'r')
print fd.read(100)
oracle.cache_return(fd)
