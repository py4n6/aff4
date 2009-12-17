import pyaff4
import time, struct, sys, gc

time.sleep(1)

oracle = pyaff4.Resolver()

date = pyaff4.XSDDatetime()
date.set(time.time())

url = pyaff4.RDFURN()
url.set("http://www.google.com")

oracle.set_value(url, pyaff4.AFF4_TIMESTAMP, date)

iter = oracle.get_iter(url, pyaff4.AFF4_TIMESTAMP)
while oracle.iter_next(iter, date):
    print "%r" % date.serialise()

url.set("file:///etc/passwd")
fd = oracle.open(url, 'r')

print fd.read(1000)


sys.exit(0)


a.parse("2009-11-10T14:12:48.000000+00:00")
print a.serialise(), time.ctime(struct.unpack("Q", a.encode())[0])

a = pyaff4.RDFURN()
a.set("http://www.google.com/")
print "%r" % a.serialise()
