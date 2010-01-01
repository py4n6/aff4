## Test the EWF reading capabililties
import pyaff4, pdb

import time
time.sleep(1)

oracle = pyaff4.Resolver()

urn = pyaff4.RDFURN()
try:
    urn.set("ewf:///var/tmp/uploads/stdcapture_0.4.pcap/stream")
    fd = oracle.open(urn, 'r')
    print fd
    fd.cache_return()
except RuntimeError:
    pass

urn.set("/var/tmp/uploads/stdcapture_0.4.pcap.e01")

if oracle.load(urn):
    print "Loaded URN %s" % urn.value

    ## Now list all the members in this volume
    stream_urn = pyaff4.RDFURN()
    iter = oracle.get_iter(urn, pyaff4.AFF4_CONTAINS)
    while oracle.iter_next(iter, stream_urn):
        print "Stream %s" % stream_urn.value
        fd = oracle.open(stream_urn, 'r')

pdb.set_trace()
