#!/usr/bin/env python
""" This is an implementation of the advanced imager described in the paper

Advanced Hash based Imaging using AFF4

Submitted to the DFRWS 2010 conference.
"""
import sk, hashlib
import pyaff4, os, sys
import pdb
import urllib

oracle = pyaff4.Resolver()

out_urn = pyaff4.RDFURN()
out_urn.set("/tmp/test3.aff4")

volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
volume.set(pyaff4.AFF4_STORED, out_urn)
volume = volume.finish()
volume_urn = volume.urn
volume.cache_return()

IN_FILENAME = "/var/tmp/uploads/testimages/winxp.E01"
in_urn = pyaff4.RDFURN()
in_urn.set(IN_FILENAME)
ZERO_URN = pyaff4.RDFURN()
ZERO_URN.set("aff4://zero")


def dump_filesystem(fd, volume_urn, image_urn):
    count = 0

    fd.seek(0)
    fs = sk.skfs(fd)
    blocksize = fs.block_size

    for root, dirs, files in fs.walk('/', unalloc=True, inodes=True):
        for f, filename in files:
            if count == 100:
                return

            count += 1
            skfs = fs.open(inode = f)
            blocks = skfs.blocks()

            ## Resident files (without a block allocation are not
            ## useful for this. Their data will be obtained when the
            ## MFT is stored anyway.
            if not blocks: continue

            m = hashlib.md5()
            ## Finish the file here
            stat = fs.fstat(skfs)
            print "Dumping %s %s (%s bytes)" % (f, filename,
                                                stat.st_size)

            sys.stdout.flush()

            while 1:
                data = skfs.read(1024*1024)
                if not data: break

                m.update(data)

            skfs.seek(0)
            hashed_urn = pyaff4.RDFURN()
            hashed_urn.set("aff4://" + m.hexdigest())

            ## Check if the hashed file already exists
            if oracle.get_id_by_urn(hashed_urn, 0):
                print "Skipping...."
            else:
                out_fd = oracle.create(pyaff4.AFF4_IMAGE)
                out_fd.urn.set(hashed_urn.value)
                out_fd.set(pyaff4.AFF4_STORED, volume_urn)
                out_fd = out_fd.finish()

                while 1:
                    data = skfs.read(1024*1024)
                    if not data: break

                    out_fd.write(data)

                out_fd.close()

            ## The following logic makes the maps more compact by only
            ## adding blocks which are not contiguous. The would have
            ## been made simpler if we could just ask the map what the
            ## next contiguous block should be, but the map would need
            ## to be sorted each time.
            file_block = 0
            last_block = -1
            for block in blocks:
                #print file_block, hashed_urn.value, block
                if last_block != block:
                    if last_block > 0:
                        image.map.add_point((last_block+1) * blocksize, 0, ZERO_URN.value)

                    image.map.add_point(block * blocksize, file_block * blocksize, hashed_urn.value)
                    print "%s,%s,%s " % (block * blocksize, file_block * blocksize, hashed_urn.value)
                    last_block = block

                file_block += 1
                last_block += 1
            print 
            ## This is the last block of the file - it may not be a full block
            extra = stat.st_size % blocksize
            #image.map.add_point(block * blocksize + extra, 0, ZERO_URN.value)

            ## Now we also add a map for the file from the image
            ## itself for convenience - this allows us to read the
            ## file, through the image, accessing the original hashed
            ## file.

            map = oracle.create(pyaff4.AFF4_MAP)
            map.urn.set(image.urn.value)
            map.urn.add(urllib.quote(filename))
            map.set(pyaff4.AFF4_STORED, volume_urn)
            map = map.finish()

            ## Update the map size to the correct value
            map.size.set(stat.st_size)

            last_block = -1
            file_block = 0
            for block in blocks:
                if last_block != block:
                    map.map.add_point(file_block * blocksize, block * blocksize, image.urn.value)
                    last_block = block

                last_block += 1
                file_block += 1

            map.close()

if oracle.load(in_urn):
    if oracle.resolve_value(in_urn, pyaff4.AFF4_CONTAINS, in_urn):
        fd = oracle.open(in_urn, "r")

        image = oracle.create(pyaff4.AFF4_MAP)
        image.urn.set(volume_urn.value)
        image.urn.add(os.path.basename(IN_FILENAME))
        image.set(pyaff4.AFF4_STORED, volume_urn)
        image = image.finish()

        image.add_point(0,0,ZERO_URN.value)
        image.size.set(fd.size.value)

        dump_filesystem(fd, volume_urn, image)

        image.map.sort()
        offset = 0
        urn = pyaff4.RDFURN()
        while 1:
            target_offset, available = image.map.get_range(offset, urn)

            if not available: break

            print offset/4096, offset, target_offset, available, urn.value

            offset += available

        image.close()

volume = oracle.open(volume_urn, 'w')
volume.close()
