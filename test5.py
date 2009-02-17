""" Test crypto support """

import fif, sys, os

TESTFILE = "test.zip"

try:
    os.unlink(TESTFILE)
except: pass

## This is the container FIF File
fiffile = fif.FIFFile()
fiffile.create_new_volume(TESTFILE)

## We create an encrypted stream within the container
enc = fiffile.create_stream_for_writing("data", stream_type='Encrypted',
                                        crypto_scheme = "PSK",
                                        passphrase = "Hello world",
                                        )

## Create a new FIF Volume inside the stream
enc_fiffile = enc.create_new_volume()

print "Encrypt a member in the FIF File"
enc_fiffile.writestr("foobar","hello world")
enc.close()

fiffile.close()

## Now to read the file - Note that this will automatically decrypt
## and merge the encrypted file
fiffile = fif.FIFFile([TESTFILE,])

## We can now access the members in the encrypted file the same as if
## they were in the container file:
print fiffile.read_member("foobar")
