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

## File will be automatically closed and properties added (including
## UUID) when GC'ed. We do not need to call fiffile.close()
