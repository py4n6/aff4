""" This is a scanner which processes pcap files """
import pyflag.Scanner as Scanner
import pyreassembler.pypcap as pypcap
import pyreassembler.reassembler as reassembler
import pyflag.Framework as Framework
import pyaff4
import pyflag.Scanner as Scanner
import pdb, sys

oracle = pyaff4.Resolver()

## A cache of pcap files
PCAP_FILE_CACHE = {}

def dissect_packet(stream_fd, stream_pkt_fd):
    """ Return a dissected packet in stream fd. Based on the current readptr.
    """
    ## What is the current range?
    offset = stream_fd.readptr
    stream_pkt_fd.seek(offset)

    urn = pyaff4.RDFURN()
    (urn, target_offset_at_point,
     available_to_read, urn_id) =  stream_pkt_fd.map.get_range(offset)

    ## Get the file from cache
    try:
        pcap_file = PCAP_FILE_CACHE[urn.value]
    except KeyError:
        pcap_fd = oracle.open(urn, 'r')
        pcap_file = pypcap.PyPCAP(pcap_fd)
        PCAP_FILE_CACHE[urn.value] = pcap_file

    if available_to_read:
        ## Go to the packet
        pcap_file.seek(target_offset_at_point)

        ## Dissect it
        try:
            return pcap_file.dissect()
        except: pass

class Reassembler:
    def make_stream(self, name, base):
        volume_urn = Framework.OUTPUT_VOLUME_URN
        forward_stream = oracle.create(pyaff4.AFF4_MAP)
        forward_stream.urn.set(volume_urn.value)
        forward_stream.urn.add(base + name)

        forward_stream.set(pyaff4.AFF4_STORED, volume_urn)
        return forward_stream.finish()

    def Callback(self, mode, packet,connection):
        try:
            self._Callback(mode, packet, connection)
        except:
            pdb.post_mortem()
            raise

    def _Callback(self, mode, packet, connection):
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
                base = "%s-%s/%s-%s/" % (
                    ip.source_addr, ip.dest_addr,
                    tcp.source, tcp.dest)

                ## Note that we hold the map locked while its in the
                ## reassembler - this prevents it from getting freed
                connection['map'] = forward_stream = self.make_stream("forward", base)
                connection['map.pkt'] = self.make_stream("forward.pkt", base)
                timestamp = pyaff4.XSDDatetime()
                timestamp.set(packet.ts_sec)
                forward_stream.set(pyaff4.AFF4_TIMESTAMP, timestamp)

                ## Make the reverse map
                connection['reverse']['map'] = reverse_stream = self.make_stream("reverse", base)
                connection['reverse']['map.pkt'] = self.make_stream("reverse.pkt", base)
                timestamp = pyaff4.XSDDatetime()
                timestamp.set(packet.ts_sec)
                reverse_stream.set(pyaff4.AFF4_TIMESTAMP, timestamp)

        elif mode == 'data':
            if tcp.data:
                if oracle.get_urn_by_id(packet.pcap_file_id, self.image_urn):
                    length = len(tcp.data)
                    connection['map'].write_from(self.image_urn, packet.offset + tcp.data_offset, length)
                    connection['map.pkt'].write_from(self.image_urn, packet.offset, length)

        elif mode == 'destroy':
            if connection['map'].size > 0 or connection['reverse']['map'].size > 0:
                map_stream = connection['map']
                map_stream_urn = map_stream.urn
                map_stream.close()

                r_map_stream = connection['reverse']['map']
                r_map_stream_urn = r_map_stream.urn
                r_map_stream.close()

                connection['map.pkt'].close()
                connection['reverse']['map.pkt'].close()

                ## Now scan the resulting with the active scanners
                Scanner.scan_urn(map_stream_urn, self.scanners)
                Scanner.scan_urn(r_map_stream_urn, self.scanners)

    def __init__(self):
        self.flush()
        self.target_id = 0
        self.targets = []
        self.image_urn = pyaff4.RDFURN()

    def flush(self):
        try:
            del self.processor
        except: pass

        self.processor = reassembler.Reassembler(packet_callback = self.Callback)

    def process(self, fd, scanners):
        self.scanners = scanners

        pcap_file = pypcap.PyPCAP(fd, file_id = oracle.get_id_by_urn(fd.urn))
        while 1:
            try:
                packet = pcap_file.dissect()
                self.processor.process(packet)
            except StopIteration:
                break

## We use a global processor. This means multiple pcap files will be
## processed as one unit. Therefore users can feed us many pcap files
## (possibly split) and we do them as if they are merged.
PROCESSOR = Reassembler()

class PCAPEvents(Framework.EventHandler):
    def finish(self):
        ## This flushes the resolver which ensures all the streams are
        ## closed
        global PROCESSOR
        PROCESSOR.flush()

class PCAPScanner(Scanner.BaseScanner):
    """ Reassemble packets in a pcap file """
    def scan(self, buffer, fd, scanners):
        if buffer.startswith("\xd4\xc3\xb2\xa1") or buffer.startswith('\xa1\xb2\xc3\xd4'):
            self.process(fd, scanners)

    def process(self, fd, scanners):
        print "Opening %s as a PCAP file" % fd.urn.parser.query
        PROCESSOR.process(fd, scanners)
