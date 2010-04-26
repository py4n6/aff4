import pytsk3
import time
import pdb

time.sleep(1)

filename ="/var/tmp/uploads/testimages/ntfs_image.dd"
img = pytsk3.AFF4ImgInfo(filename)

fs = pytsk3.FS_Info(img)

def test1(fs):
    d = pytsk3.Directory(fs, path="/")

    for f in d:
        try:
            print f.info.name.name
        except AttributeError: pass

def test2(fs):
    #f = fs.open("/Books/80day11.txt")
    f = fs.open_meta(64)
    attrs = [ a for a in f ]
    runs = [ a for a in attrs[-1] ]
    o = attrs[3].info.type
    pdb.set_trace()

test2(fs)
