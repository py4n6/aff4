import fif,os,md5,sk, sys

try:
    os.unlink("test3.zip")
except: pass

fiffile = fif.FIFFile()
fiffile.create_new_volume("test3.zip")

def build_file():
    BASEDIR = "/var/tmp/uploads/testimages/raid/linux/d%d.dd"

    for i in range(1,4):
        filename = BASEDIR % i
        fd = fiffile.create_stream_for_writing(stream_name=os.path.basename(filename))
        infd = open(filename)
        while 1:
            data = infd.read(fd.chunksize)
            if len(data)==0: break

            fd.write_chunk(data)

        fd.close()

def make_raid_map():
    blocksize = 64 * 1024
    new_stream = fiffile.create_stream_for_writing(stream_name="RAID",
                                                   stream_type='Map',
                                                   target0='d1.dd',
                                                   target1='d2.dd',
                                                   target2='d3.dd',
                                                   image_period=blocksize * 3,
                                                   file_period =blocksize * 6)
    
    new_stream.add_point(0,            0,              1)
    new_stream.add_point(1 * blocksize,0 ,             0)
    new_stream.add_point(2 * blocksize,1 * blocksize , 2)
    new_stream.add_point(3 * blocksize,1 * blocksize , 1)
    new_stream.add_point(4 * blocksize,2 * blocksize , 0)
    new_stream.add_point(5 * blocksize,2 * blocksize , 2)

    new_stream.size = 5242880 * 2
    new_stream.save_map()
    new_stream.close()
    
build_file()
make_raid_map()
fiffile.close()

fd = fiffile.open_stream("RAID")
fs = sk.skfs(fd)
f = fs.open(inode='13')
outfd = open("test3.jpg","w")
m = md5.new()
while 1:
    data = f.read(1024*1024)
    if len(data)==0: break
    outfd.write(data)
    m.update(data)

print m.hexdigest()
assert(m.hexdigest() == 'a4b4ea6e28ce5d6cce3bcff7d132f162')
