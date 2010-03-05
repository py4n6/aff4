import pyaff4
import time

time.sleep(1)

oracle = pyaff4.Resolver()

urn = pyaff4.RDFURN()
urn.set("/tmp/test.zip")

cipher = oracle.new_rdfvalue(pyaff4.AFF4_AES256_PASSWORD)
#oracle.set_value(urn, pyaff4.AFF4_CIPHER, cipher)
oracle.resolve_value(urn, pyaff4.AFF4_CIPHER, cipher)


ctext = cipher.encrypt(0,"This is a test", 16)
print "%r" % ctext
print cipher.decrypt(0, ctext, 16)


print cipher.serialise(urn)
