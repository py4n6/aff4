import pytsk3
import time

#time.sleep(1)

filename ="/var/tmp/uploads/testimages/ntfs_image.dd"
img = pytsk3.AFF4ImgInfo(filename)

fs = pytsk3.FS_Info(img)
d = pytsk3.Directory(fs, path="/")

for f in d:
    try:
        print f.info.name.name
    except AttributeError: pass
