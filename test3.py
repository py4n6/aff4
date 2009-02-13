import fif,os,md5,sk

os.unlink("test3.zip")

def build_file():
    BASEDIR = "/var/tmp/uploads/testimages/raid/linux/d%d.dd"

    for i in range(1,4):
        filename = BASEDIR % i
        try:
            fd = fif.FIFWriter("test3.zip", mode='a', stream_name=os.path.basename(filename))
        except:
            fd = fif.FIFWriter("test3.zip", mode='w', stream_name=os.path.basename(filename))
        infd = open(filename)
        while 1:
            data = infd.read(fd.chunksize)
            if len(data)==0: break

            fd.write_chunk(data)

        fd.close()

def make_raid_map():
    fd = fif.open_stream("test3.zip",'d1.dd')
    blocksize = 64 * 1024
    new_stream = fif.MapDriver(fd.fd, dict(target0='d1.dd',
                                           target1='d2.dd',
                                           target2='d3.dd',
                                           image_period=blocksize * 3,
                                           file_period =blocksize * 6),
                               "RAID")
    new_stream.add_point(0,            0,              1)
    new_stream.add_point(1 * blocksize,0 ,             0)
    new_stream.add_point(2 * blocksize,1 * blocksize , 2)
    new_stream.add_point(3 * blocksize,1 * blocksize , 1)
    new_stream.add_point(4 * blocksize,2 * blocksize , 0)
    new_stream.add_point(5 * blocksize,2 * blocksize , 2)

    new_stream.size = 5242880 * 2
    new_stream.save_map()

build_file()
make_raid_map()

fd = fif.open_stream("test3.zip","RAID")
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
