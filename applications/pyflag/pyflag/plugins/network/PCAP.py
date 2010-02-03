""" This is a scanner which processes pcap files """
import pyflag.Scanner as Scanner
import pyreassembler.pypcap as pypcap
import pyreassembler.reassembler as reassembler
import pyflag.Framework as Framework
import pyaff4
import pyflag.Scanner as Scanner

oracle = pyaff4.Resolver()

class Reassembler:
    def Callback(self, mode, packet, connection):
        volume_urn = self.volume_urn
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

                ## Create the forward stream
                forward_stream = oracle.create(pyaff4.AFF4_MAP)
                forward_stream.urn.set(volume_urn.value)
                forward_stream.urn.add(base + "forward")

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
                reverse_stream.urn.add(base + "reverse")

                reverse_stream.set(pyaff4.AFF4_STORED, volume_urn)

                timestamp = pyaff4.XSDDatetime()
                timestamp.set(packet.ts_sec)
                reverse_stream.set(pyaff4.AFF4_TIMESTAMP, timestamp)

                reverse_stream = reverse_stream.finish()

                ## Note that we hold the map locked while its in the
                ## reassembler - this prevents it from getting freed
                connection['reverse']['map'] = reverse_stream

        elif mode == 'data':
            if tcp.data:
                if oracle.get_urn_by_id(packet.pcap_file_id, self.image_urn):
                    length = len(tcp.data)
                    connection['map'].write_from(self.image_urn, packet.offset + tcp.data_offset, length)

        elif mode == 'destroy':
            if connection['map'].size > 0 or connection['reverse']['map'].size > 0:
                map_stream = connection['map']
                map_stream_urn = map_stream.urn
                map_stream.close()

                r_map_stream = connection['reverse']['map']
                r_map_stream_urn = r_map_stream.urn
                r_map_stream.close()

                ## Now scan the resulting with the active scanners
                Scanner.scan_urn(map_stream_urn, self.volume_urn, self.scanners)
                Scanner.scan_urn(r_map_stream_urn, self.volume_urn, self.scanners)

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

    def process(self, fd, volume_urn, scanners):
        self.volume_urn = volume_urn
        self.scanners = scanners

        pcap_file = pypcap.PyPCAP(fd, file_id = oracle.get_id_by_urn(fd.urn))
        while 1:
            try:
                packet = pcap_file.dissect()
                self.processor.process(packet)
            except StopIteration:
                break

## We use a global processor. This means multiple pcap files will be
## processed as one unit. Therefore users can feed up many pcap files
## (possibly split) and we do them as if they are merged.
Processor = Reassembler()

class PCAPEvents(Framework.EventHandler):
    def finish(self, outfd):
        ## This flushes the resolver which ensures all the streams are
        ## closed
        global Processor
        Processor.flush()

class PCAPScanner(Scanner.BaseScanner):
    """ Reassemble packets in a pcap file """
    def scan(self, buffer, fd, outurn, scanners):
        if buffer.startswith("\xd4\xc3\xb2\xa1") or buffer.startswith('\xa1\xb2\xc3\xd4'):
            self.process(fd, outurn, scanners)

    def process(self, fd, outurn, scanners):
        print "Opening %s as a PCAP file" % fd.urn.parser.query
        Processor.process(fd, outurn, scanners)
