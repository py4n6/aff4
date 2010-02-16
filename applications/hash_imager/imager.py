#!/usr/bin/env python
""" This is an implementation of the advanced imager described in the paper

Advanced Hash based Imaging using AFF4

Submitted to the DFRWS 2010 conference.
"""
import sk, hashlib
import pyaff4, os, sys
import pdb
import urllib
import threading

oracle = pyaff4.Resolver()

out_urn = pyaff4.RDFURN()
out_urn.set("/tmp/test3.aff4")

#IN_FILENAME = "/var/tmp/uploads/testimages/winxp.E01"
IN_FILENAME = "/var/tmp/uploads/testimages/winxp.dd"
in_urn = pyaff4.RDFURN()
in_urn.set(IN_FILENAME)

ZERO_URN = pyaff4.RDFURN()
ZERO_URN.set("aff4://zero")

class HashWorker(threading.Thread):
    def __init__(self, in_urn, volume_urn, image_stream_urn,
                 blocks, size,
                 blocksize, condition, *args, **kwargs):
        threading.Thread.__init__(self, *args, **kwargs)

        self.in_urn = in_urn
        self.volume_urn = volume_urn
        self.blocks = blocks
        self.blocksize = blocksize
        self.size = size
        self.image_stream_urn = image_stream_urn
        ## We signal this when we are done
        self.condition = condition

    def run(self):
        try:
            self.dump_block()
        finally:
            ## Signal the condition variable
            self.condition.acquire()
            self.condition.notify()
            self.condition.release()

    def dump_block(self):
        ## Read all the data into a single string
        data_blocks = []

        ## Open another read handle to the file private to our thread
        fd = oracle.open(self.in_urn, 'r')
        try:
            for block in self.blocks:
                fd.seek(block * self.blocksize)
                data_blocks.append(fd.read(self.blocksize))
        finally:
            fd.cache_return()

        data = ''.join(data_blocks)[:self.size]
        m = hashlib.md5()
        m.update(data)

        hashed_urn = pyaff4.RDFURN()
        hashed_urn.set("aff4://" + m.hexdigest())

        ## Check if the hashed file already exists
        if oracle.get_id_by_urn(hashed_urn, 0):
            print "Skipping...."
        else:
            ## Save a copy of the data
            out_fd = oracle.create(pyaff4.AFF4_IMAGE)
            out_fd.urn.set(hashed_urn.value)
            out_fd.set(pyaff4.AFF4_STORED, self.volume_urn)
            out_fd = out_fd.finish()
            out_fd.write(data)
            out_fd.close()

        ## Now make a reverse map
        ## The following logic makes the maps more compact by only
        ## adding blocks which are not contiguous. This would have
        ## been made simpler if we could just ask the map what the
        ## next contiguous block should be, but the map would need
        ## to be sorted each time.
        image_stream = oracle.open(self.image_stream_urn, 'w')
        try:
            file_block = 0
            last_block = -1
            for block in self.blocks:
                #print file_block, hashed_urn.value, block
                if last_block != block:
                    if last_block > 0:
                        image_stream.map.add_point(
                            (last_block+1) * self.blocksize, 0, ZERO_URN.value)

                    image_stream.map.add_point(
                        block * self.blocksize, file_block * self.blocksize, hashed_urn.value)

                    #print "%s,%s,%s " % (block * self.blocksize,
                    #                     file_block * self.blocksize, hashed_urn.value)
                    last_block = block

                file_block += 1
                last_block += 1

            #print
            ## This is the last block of the file - it may not be a full block
            extra = self.size % self.blocksize
            image_stream.map.add_point(block * self.blocksize + extra, 0, ZERO_URN.value)
        finally:
            image_stream.cache_return()

class HashImager:
    """ A class for creating a new hash based image.

    in_urn can be a file containing the volume, or an existing stream.
    """
    MAX_SIZE = 4000
    MAX_THREADS = 5
    workers = []

    condition = threading.Condition()

    def __init__(self, in_urn, output_urn, *args, **kwargs):
        self.in_urn = in_urn

        ## Work out what the in_urn actually is:
        if oracle.load(in_urn):
            ## It contains a volume. We just get the first stream in
            ## the volume.
            oracle.resolve_value(in_urn, pyaff4.AFF4_CONTAINS, in_urn)

        ## Check that the input urn is valid
        fd = oracle.open(in_urn, 'r')
        try:
            ## Make sure the object is a stream
            assert(isinstance(fd, pyaff4.FileLikeObject))

            ## Now make the output volume. Is it already a volume?
            if oracle.load(output_urn):
                self.volume_urn = output_urn
            else:
                ## We need to make a new volume on it:
                volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
                volume.set(pyaff4.AFF4_STORED, output_urn)
                volume = volume.finish()
                self.volume_urn = volume.urn
                volume.cache_return()

            ## We create a new output map to hold the image:
            image = oracle.create(pyaff4.AFF4_MAP)
            image.urn.set(self.volume_urn.value)
            image.urn.add(os.path.basename(IN_FILENAME))
            image.set(pyaff4.AFF4_STORED, self.volume_urn)
            image_stream = image.finish()
            self.image_urn = image_stream.urn

            ## By default we set the target to the ZERO_URN
            image_stream.add_point(0,0,ZERO_URN.value)
            image_stream.size.set(fd.size.value)
            image_stream.cache_return()
        finally:
            fd.cache_return()

        self.in_urn = in_urn

    def image(self):
        self.fd = oracle.open(self.in_urn, 'r')
        try:
            ## Create the image. First the filesystem
            self.dump_filesystem()

            ## Now analyse the holes
            self.dump_unallocated()


            ## Wait for all the workers to finish
            while len(self.workers)>0:
                self.workers = [ x for x in self.workers if x.isAlive() ]
                self.condition.acquire()
                self.condition.wait(0.5)
                self.condition.release()

            ## Done, close off all streams and output volume
            image_stream = oracle.open(self.image_urn, 'w')
            image_stream.close()

            volume = oracle.open(self.volume_urn, 'w')
            volume.close()
        finally:
            self.fd.cache_return()

    def dump_block_run(self, blocks, size):
        """ Dump the block run given as a reverse map """

        while 1:
            self.workers = [ x for x in self.workers if x.isAlive() ]
            #print self.workers
            if len(self.workers) < self.MAX_THREADS:
                ## Start a new worker
                worker = HashWorker(self.in_urn, self.volume_urn,
                                    self.image_urn, blocks, size, self.blocksize,
                                    self.condition)
                worker.start()
                #worker.join()
                self.workers.append(worker)
                return
            else:
                self.condition.acquire()
                self.condition.wait(0.5)
                self.condition.release()

    def dump_filesystem(self):
        """ This method analyses the filesystem and dumps all the
        files in it based on hashes.
        """
        count = 0

        ## FIXME - handle partitions here
        self.fd.seek(0)
        fs = sk.skfs(self.fd)
        self.blocksize = fs.block_size

        for root, dirs, files in fs.walk('/', unalloc=False, inodes=True):
            for f, filename in files:
#                if count == 100:
#                    return

                count += 1
                skfs = fs.open(inode = f)
                blocks = skfs.blocks()

                ## Resident files (without a block allocation are not
                ## useful for this. Their data will be obtained when the
                ## MFT is stored anyway.
                if not blocks: continue

                ## Calculate the hash here
                stat = fs.fstat(skfs)
                ## Dont do very small files
                if stat.st_size < self.blocksize: continue

                ## If the file is compressed we dont bother doing it -
                ## reading ntfs compressed files is too slow.
                if stat.st_size > (len(blocks)+1)*self.blocksize: continue

                print "Dumping %s %s (%s bytes)" % (f, filename,
                                                    stat.st_size)

                sys.stdout.flush()


                i = 0
                size = stat.st_size
                while 1:
                    run = blocks[i:i+self.MAX_SIZE]
                    if not run: break

                    if len(run) == self.MAX_SIZE:
                        size -= self.blocksize * self.MAX_SIZE

                    self.dump_block_run(run, size)
                    i+=self.MAX_SIZE

                continue
                ## Now we also add a map for the file from the image
                ## itself for convenience - this allows us to read the
                ## file, through the image, accessing the original hashed
                ## file.
                map = oracle.create(pyaff4.AFF4_MAP)
                map.urn.set(self.image_urn.value)
                map.urn.add(urllib.quote(filename))
                map.set(pyaff4.AFF4_STORED, self.volume_urn)
                map = map.finish()

                ## Update the map size to the correct value
                map.size.set(stat.st_size)

                last_block = -1
                file_block = 0
                for block in blocks:
                    if last_block != block:
                        map.map.add_point(file_block * self.blocksize, block * self.blocksize, self.image_urn.value)
                        last_block = block

                    last_block += 1
                    file_block += 1

                map.close()

    def dump_unallocated(self):
        return
        image_stream = oracle.open(self.image_urn, 'w')
        try:
            image_stream.map.sort()
            offset = 0
            urn = pyaff4.RDFURN()
            while 1:
                target_offset, available = self.image_stream.map.get_range(offset, urn)

                if not available: break

                print offset/4096, offset, target_offset, available, urn.value

                offset += available
        finally:
            image_stream.cache_return()

## Shut up messages
class Renderer:
    def message(self, level, message):
        pass

oracle.set_logger(pyaff4.ProxiedLogger(Renderer()))

imager = HashImager(in_urn, out_urn)
imager.image()
