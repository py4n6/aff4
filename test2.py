import sk,fif,sys

FILENAME = 'test.00.zip'
fiffile = fif.FIFFile([FILENAME])

def add_map():
    fd = fiffile.open_stream("data")
    fs = sk.skfs(fd)
    f = fs.open('/RAW/logfile1.txt')
    
    ## We want to append to the last volume:
    fiffile.append_volume(FILENAME)

    ## Create a new Map stream
    new_stream = fiffile.create_stream_for_writing("logfile1.txt",
                                                   stream_type = 'Map',
                                                   target = 'data')

    count = 0
    block_size = fs.block_size
    for block in f.blocks():
        new_stream.add_point(count * block_size, block * block_size, 0)
        count += 1

    new_stream.pack()
    f.seek(0,2)
    new_stream.size = f.tell()
    new_stream.save_map()
    new_stream.close()

add_map()

test_fd = fiffile.open_stream("logfile1.txt")
fd = fiffile.open_stream("data")

fs = sk.skfs(fd)
f = fs.open('/RAW/logfile1.txt')

BLOCKSIZE = 1024*1024
while 1:
    test_data = test_fd.read(BLOCKSIZE)
    if len(test_data)==0: break

    data = f.read(BLOCKSIZE)
    if data!=test_data:
        print len(data), len(test_data)
    assert(data == test_data)

fiffile.close()

