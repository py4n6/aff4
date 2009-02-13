import sk,fif,sys

def add_map():
    fd = fif.open_stream("test.zip")
    fs = sk.skfs(fd)
    f = fs.open('/RAW/logfile1.txt')

    new_stream = fif.MapDriver(fd.fd, dict(target0 = 'data'), "logfile1.txt")

    count = 0
    block_size = fs.block_size
    for block in f.blocks():
        new_stream.add_point(count * block_size, block * block_size, 0)
        count += 1

    new_stream.pack()
    f.seek(0,2)
    new_stream.size = f.tell()
    new_stream.save_map()

add_map()
#sys.exit(0)

test_fd = fif.open_stream("test.zip", "logfile1.txt")
fd = fif.open_stream("test.zip")
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
