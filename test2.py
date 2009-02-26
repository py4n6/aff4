"""
This test tests the Map stream type - we use SK to extract a block
allocation list for the file and make a map stream.. We then read the
streams directly and compare to what it should be.

You should have an image of ntfs1-gen2.dd created as per test1 in samples. The
name should be ntfs1-gen2-mapinside.00.zip
"""

import sk,fif,sys

FILENAME = './samples/ntfs1-gen2-mapinside.00.zip'
fiffile = fif.FIFFile([FILENAME], None, False)


def add_map():

    images = fiffile.get_images()
    global originalImage
    originalImage = images[0]
    fd = fiffile.open_stream(originalImage)
    
    fs = sk.skfs(fd)
    f = fs.open('/RAW/logfile1.txt')
    
    ## We want to append to the last volume:
    fiffile.append_volume(FILENAME)

    ## Create a new Map stream
    new_stream = fiffile.create_stream_for_writing(
                                                   stream_type = 'aff2:Map',
                                                   target = originalImage)
    new_stream.properties["aff2:name"] = "logfile1.txt"
    fiffile.properties["aff2:containsImage"] = new_stream.getId()
    global mappedStreamID
    mappedStreamID = new_stream.getId()
    print "Mapped ID = %s" % mappedStreamID
    count = 0
    block_size = fs.block_size
    ## Build up the mapping function
    for block in f.blocks():
        new_stream.add_point(count * block_size, block * block_size, 0)
        count += 1

    new_stream.pack()
    f.seek(0,2)
    new_stream.size = f.tell()
    new_stream.save_map()
    new_stream.close()

add_map()

## Now we compare the mapped stream with the stream produced by SK:
print "Mapped ID = %s" % mappedStreamID
test_fd = fiffile.open_stream(mappedStreamID)
fd = fiffile.open_stream(originalImage)

fs = sk.skfs(fd)
f = fs.open('/RAW/logfile1.txt')

## Do big reads - sk is very slow with compressed ntfs files
BLOCKSIZE = 1024*1024
while 1:
    test_data = test_fd.read(BLOCKSIZE)
    if len(test_data)==0: break

    data = f.read(BLOCKSIZE)
    if data!=test_data:
        print len(data), len(test_data)
    assert(data == test_data)

fiffile.close()
