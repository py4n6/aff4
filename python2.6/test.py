import pyaff4
import time, struct, sys, gc, pdb

time.sleep(1)

oracle = pyaff4.Resolver()
url = pyaff4.RDFURN()
url.set("file:///tmp/test.zip")

zip_fd = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
oracle.set_value(zip_fd.urn, pyaff4.AFF4_STORED, url)
zip_fd = zip_fd.finish()

image_fd = oracle.create(pyaff4.AFF4_IMAGE)
oracle.set_value(image_fd.urn, pyaff4.AFF4_STORED, zip_fd.urn)
image_fd = image_fd.finish()

fd = open("/bin/ls")
while 1:
    data = fd.read(1e6)
    if not data: break

    image_fd.write(data)

image_fd.close()
zip_fd.close()

sys.exit(0)


a.parse("2009-11-10T14:12:48.000000+00:00")
print a.serialise(), time.ctime(struct.unpack("Q", a.encode())[0])

a = pyaff4.RDFURN()
a.set("http://www.google.com/")
print "%r" % a.serialise()
