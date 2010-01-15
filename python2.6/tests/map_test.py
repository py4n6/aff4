import pyaff4
import pdb, os
import time, sys

#time.sleep(1)

DD_DIR = "/var/tmp/uploads/testimages/raid/linux/"

d1 = DD_DIR + "d1.dd"
d2 = DD_DIR + "d2.dd"
d3 = DD_DIR + "d3.dd"

oracle = pyaff4.Resolver()

target_urn = pyaff4.RDFURN()
target_urn.set("/bin/ls")

volume_urn = pyaff4.RDFURN()
volume_urn.set("/tmp/foobar.zip")

try:
    os.unlink(volume_urn.parser.query)
except: pass

## New volume
volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME,'w')
oracle.set_value(volume.urn, pyaff4.AFF4_STORED, volume_urn)
volume = volume.finish()

volume_urn = volume.urn
volume.cache_return()

print volume_urn.value

map = oracle.create(pyaff4.AFF4_MAP,'w')
oracle.set_value(map.urn, pyaff4.AFF4_STORED, volume_urn);

blocksize = 64 * 1024

i = pyaff4.XSDInteger()

i.set(3 * blocksize)
oracle.set_value(map.urn, pyaff4.AFF4_TARGET_PERIOD, i)

i.set(6 * blocksize)
oracle.set_value(map.urn, pyaff4.AFF4_IMAGE_PERIOD, i)

i.set(5242880 * 2)
oracle.set_value(map.urn, pyaff4.AFF4_SIZE, i)

map = map.finish()

size = map.size

map.add(0 * blocksize,0 * blocksize,d2)
map.add(1 * blocksize,0 * blocksize,d1)
map.add(2 * blocksize,1 * blocksize,d3)
map.add(3 * blocksize,1 * blocksize,d2)
map.add(4 * blocksize,2 * blocksize,d1)
map.add(5 * blocksize,2 * blocksize,d3)

map_urn = map.urn

map.close()

sys.exit(0)

volume = oracle.open(volume_urn, 'w')
volume.close()

fd = oracle.open(map_urn, 'r')
outfd = open("/tmp/output.dd","w")
while 1:
    data = fd.read(1000)
    if not data: break

    outfd.write(data)


outfd.close()
fd.cache_return()
