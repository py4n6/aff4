import pyaff4
import time, struct, sys, gc, pdb, os

time.sleep(1)

oracle = pyaff4.Resolver()
volume_urn = pyaff4.RDFURN()
volume_urn.set("/tmp/test.zip")


try:
    os.unlink(volume_urn.parser.query)
except Exception,e:
    print e


volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
oracle.set_value(volume.urn, pyaff4.AFF4_STORED, volume_urn)
volume = volume.finish()

volume_urn = volume.urn
volume.cache_return()

image_fd = oracle.create(pyaff4.AFF4_IMAGE)
oracle.set_value(image_fd.urn, pyaff4.AFF4_STORED, volume_urn)
image_fd = image_fd.finish()
image_urn = image_fd.urn

fd = open("/bin/ls")
while 1:
    data = fd.read(1000000)
    if not data: break

    image_fd.write(data)

image_fd.close()

volume = oracle.open(volume_urn, 'w')
volume.close()

fd = oracle.open(image_urn, 'r')
outfd = open("/tmp/output.dd","w")
while 1:
    data = fd.read(1000)
    if not data: break

    outfd.write(data)


outfd.close()
fd.cache_return()

sys.exit(0)


a.parse("2009-11-10T14:12:48.000000+00:00")
print a.serialise(), time.ctime(struct.unpack("Q", a.encode())[0])

a = pyaff4.RDFURN()
a.set("http://www.google.com/")
print "%r" % a.serialise()
