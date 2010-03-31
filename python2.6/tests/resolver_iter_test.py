import pyaff4
import time
time.sleep(1)

oracle = pyaff4.Resolver()

URN = "aff4://navigation/root"
URN = "aff4://69581d02-36ae-47a4-b0ba-888368dc2e11/192.168.1.34-192.168.1.1/38099-25/reverse"

urn = pyaff4.RDFURN()
urn.set(URN)

attribute = pyaff4.XSDString()

iter = pyaff4.RESOLVER_ITER()
next = 0

while oracle.attributes_iter(urn, attribute, iter):
    print
    print attribute.value
    while 1:
        obj = oracle.alloc_from_iter(iter)
        if not obj: break

        print "    -> type (%s) " % (obj.dataType)
        print "    -> data (%s) " % (obj.serialise(urn))
