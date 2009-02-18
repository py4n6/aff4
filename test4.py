""" This tests the underlying zipfile implementation in FIF files. The
regular zipfile implementation is designed to have one reader or
writer at the time. There is no support for concurrent access between
readers and writers. FIF files may be opened for reading or writing at
the same time.
"""

import fif,os

TESTFILE = "test4.zip"

try:
    os.unlink(TESTFILE)
except: pass

## Create a new volume for writing on
fiffile = fif.FIFFile()
fiffile.create_new_volume(TESTFILE)

## Write a new member
fiffile.writestr("testfile", "This is a test")

## We can access the new member right away
print fiffile.read_member("testfile")

## We can add a new member now:
fiffile.writestr("testfile2", "This is another test")

## And access that right away
print fiffile.read_member("testfile2")

## We can update a new member
fiffile.writestr("testfile", "This is a test - new version")

## And see the change right away
print fiffile.read_member("testfile")

## We can get a filelike object for writing on
fd = fiffile.open_member("test stream",'w')

## Write some data
fd.write("hello world")

## This should raise - can not open the same fif file for writing
## twice (only a single fif writer allowed at the time, but multiple
## readers):
try:
    fiffile.open_member("another stream",'w')
    print "Hmm should have raised here"
except IOError: pass

## Now close the stream object - this should make it available to
## reading and allow a new stream to be opened.
fd.close()

## We should not be able to see the data until its complete
print fiffile.read_member("test stream")

## File will be automatically closed and properties added (including
## UUID) when GC'ed. We do not need to call fiffile.close() although
## its a good idea.
