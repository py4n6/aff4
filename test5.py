""" Test crypto support.

This file tests the ability to create encrypted volumes.
"""

import fif, sys, os

TESTFILE = "test5.zip"

try:
    os.unlink(TESTFILE)
except: pass

## This is the container FIF File
fiffile = fif.FIFFile()
fiffile.create_new_volume(TESTFILE)

## We create an encrypted stream within the container
enc = fiffile.create_stream_for_writing("data", stream_type='Encrypted',
                                        scheme = "aes-sha-psk",
                                        ## We can provide the
                                        ## passphrase directly here -
                                        ## or else the PSK driver will
                                        ## ask for it later:
                                        PSK = "a",
                                        )

## Create a new FIF Volume inside the stream - all new writes can take
## place to the enc_fiffile volume:
enc_fiffile = enc.create_new_volume()

print "Encrypt a member in the FIF File"
enc_fiffile.writestr("foobar","hello world")
## Closing the stream causes the contained fif file to be also closed
## - enc_fiffile should not be used any more.
enc.close()

## Closing the container file finalises the zip file.
fiffile.close()

## Now to read the file - Note that this will automatically decrypt
## and merge the encrypted file
fiffile = fif.FIFFile(TESTFILE)

## We can now access the members in the encrypted file the same as if
## they were in the container file:
print fiffile.read_member("foobar")
