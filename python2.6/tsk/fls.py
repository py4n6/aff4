#!/usr/bin/python

import pytsk3
from optparse import OptionParser
import sys
import pdb

parser = OptionParser()
parser.add_option("-f", "--fstype", default=None,
                  help="File system type (use '-f list' for supported types)")

parser.add_option('-o', '--offset', default=0, type='int',
                  help='Offset in the image (in bytes)')
                  
parser.add_option("-l", "--long", action='store_true', default=False,
                  help="Display long version (like ls -l)")

(options, args) = parser.parse_args()

def error(string):
    print string
    sys.exit(1)

try:
    url = args[0]
except IndexError:
    error("You must specify an image (try '%s -h' for help)" % sys.argv[0])

if len(args)==1:
    inode = 0
    path = '/'
elif len(args)==2:
    try:
        inode = int(args[1])
        path = None
    except:
        inode = 0
        path = args[1]

else:
    error("Too many arguements provided")

## Now list the actual files (any of these can raise for any reason)

## Step 1: get an IMG_INFO object (url can be any URL that AFF4 can
## handle)
img = pytsk3.AFF4ImgInfo(url, offset=options.offset)

## Step 2: Open the filesystem
fs = pytsk3.FS_Info(img)

## Step 3: Open the directory node
if path:
    directory = fs.open_dir(path=path)
else:
    directory = fs.open_dir(inode=inode)

## Step 4: Iterate over all files in the directory and print their
## name. What you get in each iteration is a proxy object for the
## TSK_FS_FILE struct - you can further dereference this struct into a
## TSK_FS_NAME and TSK_FS_META structs.
for f in directory:
    try:
        print f.info.meta.size, f.info.name.name
    except Exception,e:
        print e, f.info.name.name
