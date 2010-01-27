""" This tests the locking mechanism between two different processes """
import pyaff4
import os, time, pdb
import threading

oracle = pyaff4.Resolver()
urn = pyaff4.RDFURN()
urn.set("/tmp/foobar.zip")

time.sleep(1)

urn2 = pyaff4.RDFURN()
urn2.set("/tmp/foobar.zip.tmp")

try:
    os.unlink(urn.parser.query)
except: pass

def log(msg):
    print "0x%X: %s" % (0xFFFFFFFFL & threading.current_thread().ident, msg)

zip = oracle.create(pyaff4.AFF4_ZIP_VOLUME, 'w')
oracle.set_value(zip.urn, pyaff4.AFF4_STORED, urn)
zip = zip.finish()
zip_urn = zip.urn
zip.cache_return()

def a():
    log( "parent started")
    zip = oracle.open(zip_urn, 'w')
    try:
        log( "Parent: Opening")
        segment1 = zip.open_member("hello", "w", 0)
        log( "Parent: Starting write")
    finally:
        zip.cache_return()

    time.sleep(2)
    segment1.write("hello")
    segment1.close()
    log( "Parent: Ended write")

    zip = oracle.open(zip_urn, 'w')
    zip.close()

def b():
    zip = oracle.open(zip_urn, 'w')
    try:
        log( "Child: Opening")
        segment2 = zip.open_member("world", "w", 0)
        log( "Child: Starting write")
    finally:
        zip.cache_return()

    time.sleep(3)
    segment2.write("hello")
    segment2.close()
    log( "Child: Ended write")
    zip = oracle.open(zip_urn, 'w')
    zip.close()

pid = os.fork()
if pid:
    ## Parent
    a()
else:
    ## child
    b()
