import pyaff4
import time, pdb
time.sleep(1)

oracle = pyaff4.Resolver()

urn = pyaff4.RDFURN()
urn.set("/tmp/foobar.zip")

def fail(urn):
    """ This fails due to deadlocks. """
    zip = oracle.create(pyaff4.AFF4_ZIP_VOLUME, 'w')
    oracle.set_value(zip.urn, pyaff4.AFF4_STORED, urn)
    zip = zip.finish()

    try:
        segment1 = zip.open_member("hello", "w")
        segment2 = zip.open_member("world", "w")
        segment1.write("hello")
        segment2.write("world")
        segment1.close()
        segment2.close()
    finally:
        zip.close()

def passed(urn):
    #this works
    zip = oracle.create(pyaff4.AFF4_ZIP_VOLUME, 'w')
    oracle.set_value(zip.urn, pyaff4.AFF4_STORED, urn)
    zip = zip.finish()

    segment1 = zip.open_member("hello", "w")
    segment1.write("hello")
    segment1.close()

    segment2 = zip.open_member("world", "w")
    segment2.write("world")
    segment2.close()
    zip.close()

passed(urn)
