import fif,os

TESTFILE = "test.zip"

try:
    os.unlink(TESTFILE)
except: pass

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
fd = fiffile.open_stream_for_writing("test stream")

## Write some data
fd.write("hello world")

## This should raise - can no open the same file for writing twice:
try:
    fiffile.open_stream_for_writing("another stream")
    print "Hmm should have raised here"
except IOError: pass

fd.close()

## We should not be able to see the data until its complete
print fiffile.read_member("test stream")

## File will be automatically closed and properties added (including
## UUID) when GC'ed. We do not need to call fiffile.close()
