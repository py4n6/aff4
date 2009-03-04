"""
This test tests the Map stream type - we use SK to extract a block
allocation list for the file and make a map stream.. We then read the
streams directly and compare to what it should be.

You should have an image set created as in test.py
"""

import sk,fif,sys

inf = fif.InfoStore()

fiffile = fif.FIFFile(["./samples/ntfs1-gen2.00.zip"], infostore=inf)
mapfile = fif.FIFFile(["./samples/testmap.00.zip"], resolver = fiffile.resolver, infostore=inf)

## Now we compare the mapped stream with the stream produced by SK:
image_fd = fiffile.open_stream_by_name("./images/ntfs1-gen2.dd")
map_fd = mapfile.open_stream_by_name("logfile1.txt")

fs = sk.skfs(image_fd)
f = fs.open('/RAW/logfile1.txt')

## Do big reads - sk is very slow with compressed ntfs files
BLOCKSIZE = 1024*1024
while 1:
    map_data = map_fd.read(BLOCKSIZE)
    if len(map_data)==0: break

    sk_data = f.read(BLOCKSIZE)
    if map_data!=sk_data:
        print len(map_data), len(sk_data)

    assert(map_data == sk_data)

print "OK."
fiffile.close()
mapfile.close()
