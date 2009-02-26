"""
This test creates a Map stream in its own container, which references
the original image. As per test 2, We use SK to extract a block
allocation list for the file and make a map stream. However teh map stream
is stored in testmap.oo.zip
. We then read the

You should have an image set created as in test2.py
"""

import sk,fif,sys

FILENAME = './samples/ntfs1-gen2.00.zip'
fiffile = fif.FIFFile([FILENAME])
mapfile = fif.FIFFile()

def add_map():

    images = fiffile.get_images()
    print images
    global originalImage
    originalImage = images[0]
    fd = fiffile.open_stream(originalImage)
    
    fs = sk.skfs(fd)
    f = fs.open('/RAW/logfile1.txt')
    
    ## We want to append to the last volume:
    fiffile.append_volume(FILENAME)
    count = 0
    new_name = "%s.%02d.zip" % ("./samples/testmap", count)
    
    mapfile.create_new_volume(new_name)
    ## Create a new Map stream
    new_stream = mapfile.create_stream_for_writing(stream_type = 'aff2:Map',
                                                   target = originalImage)
    new_stream.properties["aff2:name"] = "logfile1.txt"
    mapfile.properties["aff2:containsImage"] = new_stream.getId()
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

fiffile.close()

mapfile.close()
                                



