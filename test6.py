""" This tests the Overlay Stream.

We generate an Overlay from test.E01 in the current directory and then
use SK to read a file from it.

We expect to have a patched ./ewfinfo that prints the chunk layout to
stdout. We also need the sk python binding.

"""
import pyewf,subprocess, fif,os

FILENAME = "test6.zip"

try:
    os.unlink(FILENAME)
except: pass

fiffile = fif.FIFFile()
fiffile.create_new_volume(FILENAME)

fd = fiffile.create_stream_for_writing(stream_type='Overlay')

p = subprocess.Popen(['./ewfinfo','-c','test.E01'], stdout=subprocess.PIPE)

for line in p.stdout.xreadlines():
    line = line.strip()
    if "," in line:
        id, offset, size, compression, filename = line.split(",",5)
        id = int(id)
        offset = long(offset)
        size = long(size)
        compression = int(compression)
        fd.set_chunk(id, offset, size, compression, filename)
    elif "=" in line:
        key, value = line.split("=",1)
        if key=='size':
            fd.size = int(value)
        fd.properties.set(key, value)

fd.close()

test_fd = fiffile.open_stream("default")
fd = pyewf.open(["test.E01",])

#test_fd.seek(-10,2)
#fd.seek(-10,2)

#print len(test_fd.read(10))
#print len(fd.read(10))

## Do big reads - sk is very slow with compressed ntfs files
BLOCKSIZE = 1024*1024
while 1:
    test_data = test_fd.read(BLOCKSIZE)
    if len(test_data)==0: break

    data = fd.read(BLOCKSIZE)
    if data!=test_data:
        print len(data), len(test_data)
    assert(data == test_data)


fiffile.close()
