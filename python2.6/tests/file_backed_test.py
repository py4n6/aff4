import pyaff4
import time

time.sleep(1)

oracle = pyaff4.Resolver()

urn = pyaff4.RDFURN()
urn.set("/etc/passwd")

fd = oracle.open(urn, 'r')
while 1:
    line = fd.readline()
    if not line: break

    print "%r" % line
