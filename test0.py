""" This is a test of the ability to read an image from a FIF File.

Depends on samples\mediumimage.00.zip, which contains an
image file containing merely test. 

"""

import sk, fif, pyewf
import time, sys, glob

def main():
    if 1:
        fiffile = fif.FIFFile("./samples/mediumimage.00.zip", readwrite=True)
        
        fd = fiffile.open_stream_by_name("mediumimage.dd")
        count = 0
        while 1:
            data=fd.read(1024*1024*30)
            if len(data)==0: break

            count+=len(data)
        assert(count == 92208)
        print "OK."

main()
sys.exit(0)



