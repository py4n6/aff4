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
enc_fiffile = fif.FIFFile()
enc_fiffile.create_new_volume(enc)

## Encrypt a member in the FIF File
enc_fiffile.writestr("foobar","hello world")

## Must call close here explicitly to guarantee closing occurs in this
## order (innermost to outermost)
enc_fiffile.close()
enc.close()
fiffile.close()

## Now to read the file
fiffile = fif.FIFFile([TESTFILE,])
enc = fiffile.open_stream("data")

open('output.bin','w').write(enc.read())
enc = fiffile.open_stream("data")

enc_fiffile = fif.FIFFile([enc])
print enc_fiffile.read_member("foobar")
