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

import time
#time.sleep(1)

oracle = pyaff4.Resolver()

out_urn = pyaff4.RDFURN()
out_urn.set("/tmp/test3.aff4")

IN_FILENAME = "/var/tmp/uploads/testimages/winxp.E01"
IN_FILENAME = "/var/tmp/uploads/testimages/winxp.dd"
#IN_FILENAME = "/var/tmp/uploads/testimages/ntfs_image.dd"
in_urn = pyaff4.RDFURN()
in_urn.set(IN_FILENAME)

ZERO_URN = pyaff4.RDFURN()
ZERO_URN.set("aff4://zero")

def log(x):
    print "0x%lX: %s" % (threading.current_thread().ident & 0xFFFFFFFF, x)

class HashWorker(threading.Thread):
    def __init__(self, in_urn, volume_urn, slack_urn,
                 image_stream_urn,
                 blocks, blocksize,
                 condition, *args, **kwargs):
        threading.Thread.__init__(self, *args, **kwargs)

        self.in_urn = in_urn
        self.volume_urn = volume_urn
        self.blocks = blocks
        self.blocksize = blocksize
        self.image_stream_urn = image_stream_urn
        ## We signal this when we are done
        self.condition = condition
        self.slack_urn = slack_urn
        self.inode = None

    def run(self):
        try:
            self.dump_blocks()
        finally:
            ## Signal the condition variable
            self.condition.acquire()
            self.condition.notify()
            self.condition.release()

    def dump_blocks(self):
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

        data = ''.join(data_blocks)
        #m = hashlib.md5()
        m = hashlib.sha1()
        m.update(data)

        hashed_urn = pyaff4.RDFURN()
        hashed_urn.set("aff4://" + m.hexdigest())

        ## Check if the hashed file already exists
        if oracle.get_id_by_urn(hashed_urn, 0):
            log("Skipping.... %s" % hashed_urn.value)
        else:
            ## Save a copy of the data
            out_fd = oracle.create(pyaff4.AFF4_IMAGE)
            out_fd.urn.set(hashed_urn.value)
            out_fd.set(pyaff4.AFF4_STORED, self.volume_urn)
            out_fd = out_fd.finish()
            out_fd.write(data)
            out_fd.close()

        if self.inode:
            string = pyaff4.XSDString()
            string.set(self.inode)
            #log("Setting %s filename %s" % (hashed_urn.value, self.inode))
            oracle.add_value(hashed_urn, pyaff4.PREDICATE_NAMESPACE + "filename", string)

        self.add_block_run(hashed_urn)

    def add_block_run(self, hashed_urn):
        image_stream = oracle.open(self.image_stream_urn, 'w')
        available = 0
        try:
            #log("Acquired %s" % self.image_stream_urn.value)
            blocks = self.blocks + [-1]
            last_image_block = -10

            for file_block in range(len(blocks)-1):
                file_offset = file_block * self.blocksize
                img_block = blocks[file_block]
                image_offset = img_block * self.blocksize

                ## First we write the start of each block
                if last_image_block + 1 != img_block:
                    available = image_stream.map.add_point(image_offset,
                                                           file_offset, hashed_urn.value)
                    #log("Adding %s,%s,%s " % (image_offset,
                    #                          file_offset, hashed_urn.value))
                else:
                    available -= self.blocksize

                last_image_block = img_block

                ## Now check the end of the range. If there is more
                ## that a block available after the start of the
                ## block, obviously its us. If the next img_block is
                ## not consecutive, we need to terminate it off with a
                ## zero urn.
                if available > self.blocksize and img_block +1 != blocks[file_block+1]:
                    ## Add a zero urn to stop off this block
                    image_stream.map.add_point(image_offset + self.blocksize,
                                               0, ZERO_URN.value)

                    #log("Adding %s,%s,%s " % (image_offset + self.blocksize,
                    #                          0, ZERO_URN.value))

        finally:
            image_stream.cache_return()
            #log("Returned %s" % self.image_stream_urn.value)

class HashImager:
    """ A class for creating a new hash based image.

    in_urn can be a file containing the volume, or an existing stream.
    """
    ## The maximum number of blocks in a single run
    MAX_SIZE = 10000

    ## The minimum number of blocks in a single run. If files are less
    ## than this they will not be compressed in their own segments. If
    ## unallocated blocks are smaller than this they will all be
    ## merged into the miscelaneous stream.
    MIN_SIZE = 64

    MAX_THREADS = 2
    workers = []

    ## A condition variable for threads to sync on
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
            #image.set_data_type(pyaff4.AFF4_MAP_TEXT)
            image.urn.set(self.volume_urn.value)
            image.urn.add(os.path.basename(IN_FILENAME))
            image.set(pyaff4.AFF4_STORED, self.volume_urn)
            image_stream = image.finish()

            self.image_urn = image_stream.urn

            ## By default we set the target to the ZERO_URN
            image_stream.add_point(0,0,ZERO_URN.value)
            image_stream.size.set(fd.size.value)
            image_stream.cache_return()

            ## Create a stream for slack:
            slack = oracle.create(pyaff4.AFF4_IMAGE)
            slack.urn.set(self.image_urn.value)
            slack.urn.add("slack")
            slack.set(pyaff4.AFF4_STORED, self.volume_urn)
            slack = slack.finish()

            self.slack_urn = slack.urn
            slack.cache_return()
        finally:
            fd.cache_return()

        self.in_urn = in_urn

    def test(self):
        self.blocksize = 1000
        self.fd = oracle.open(self.in_urn, 'r')
        try:
            self.dump_block_run([0,2,3], 0)
            self.dump_block_run([4,6,7], 0)
            self.dump_block_run([5,8,9,1], 0)

            self.join_workers()

            ## Done, close off all streams and output volume
            #log("Closing image_stream")
            image_stream = oracle.open(self.image_urn, 'w')
            image_stream.close()

            slack = oracle.open(self.slack_urn, 'w')
            slack.close()

            volume = oracle.open(self.volume_urn, 'w')
            volume.close()

        finally:
            self.fd.cache_return()

    def join_workers(self):
        ## Wait for all the workers to finish
        while len(self.workers)>0:
            self.workers = [ x for x in self.workers if x.isAlive() ]
            self.condition.acquire()
            self.condition.wait(0.5)
            self.condition.release()

    def image(self):
        self.fd = oracle.open(self.in_urn, 'r')
        try:
            ## Create the image. First the filesystem
            self.dump_filesystem()

            ## Wait for all the files to be written
            self.join_workers()

            ## Now analyse the holes
            self.dump_unallocated()

            self.join_workers()

            ## Done, close off all streams and output volume
            #log("Closing image_stream")
            image_stream = oracle.open(self.image_urn, 'w')
            image_stream.close()

            slack = oracle.open(self.slack_urn, 'w')
            slack.close()

            volume = oracle.open(self.volume_urn, 'w')
            volume.close()
        finally:
            self.fd.cache_return()

    def dump_block_run(self, blocks, inode=None):
        """ Dump the block run given as a reverse map """

        while 1:
            self.workers = [ x for x in self.workers if x.isAlive() ]
            #print self.workers
            if len(self.workers) < self.MAX_THREADS:
                ## Start a new worker
                worker = HashWorker(self.in_urn, self.volume_urn,
                                    self.slack_urn,
                                    self.image_urn, blocks,
                                    self.blocksize,
                                    self.condition)
                worker.inode = inode

                worker.start()
                #worker.join()
                self.workers.append(worker)
                #log( "spawned 0x%X" % (0xFFFFFFFF & worker.ident))
                return
            else:
                self.condition.acquire()
                self.condition.wait(0.5)
                self.condition.release()

    def dump_blocks(self, blocks, filename=None):
        i = 0
        while 1:
            ## Split the file into smaller runs
            if len(blocks) - i > self.MAX_SIZE:
                run = blocks[i:i+self.MAX_SIZE]
                self.dump_block_run(run, inode = filename)
                i += self.MAX_SIZE
            else:
                run = blocks[i:]
                self.dump_block_run(run, inode = filename)
                break

    def dump_filesystem(self):
        """ This method analyses the filesystem and dumps all the
        files in it based on hashes.
        """
        ## FIXME - handle partitions here
        self.fd.seek(0)
        fs = sk.skfs(self.fd)
        self.blocksize = fs.block_size

        for root, dirs, files in fs.walk('/', unalloc=False, inodes=True):
            for f, filename in files:
                skfs = fs.open(inode = f)
                blocks = skfs.blocks()

                if len(blocks) > self.MIN_SIZE:
                    log("Dumping %s %s (%s bytes)" % (f, filename,
                                                      len(blocks) * self.blocksize))
                    self.dump_blocks(blocks, filename)

    def dump_file_name(self):
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
                map.map.add_point(file_block * self.blocksize,
                                  block * self.blocksize,
                                  self.image_urn.value)
                last_block = block

            last_block += 1
            file_block += 1

        map.close()

    def dump_unallocated(self):
        offset = 0
        blocks = []

        while 1:
            image_stream = oracle.open(self.image_urn, 'w')
            try:
                urn, target_offset, available, target_id = image_stream.map.get_range(offset)
            finally:
                image_stream.cache_return()

            if not available: break

            ## We only want gaps
            #log( "Checking map point %s,%s,%s,%s" % (offset, target_offset, available, urn.value))

            if urn.value == ZERO_URN.value:
                ## At this point the gaps should be even blocksizes.
                if (available % self.blocksize) != 0 or (offset % self.blocksize) != 0:
                    pdb.set_trace()
                    print "????"

                log( "Dumping unallocated from %s to %s (%s bytes)" % (
                        offset, offset+available, available))
                blocks += [ x for x in range(offset / self.blocksize,
                                             (offset + available) / self.blocksize) ]
                if len(blocks) >= self.MAX_SIZE:
                    self.dump_blocks(blocks[:self.MAX_SIZE])
                    blocks = blocks[self.MAX_SIZE:]

            offset += available

        self.dump_blocks(blocks)
## Shut up messages
class Renderer:
    def message(self, level, message):
        pass

oracle.set_logger(pyaff4.ProxiedLogger(Renderer()))

imager = HashImager(in_urn, out_urn)
imager.image()
#imager.test()

sys.exit(0)

fd = oracle.open(imager.image_urn, 'r')
log( "\nMap %s" % fd.urn.value)
log( "---------\n\n")

offset = 0
while 1:
    urn, target_offset, available, target_id = fd.map.get_range(offset)
    log("%s,%s,%s,%s"%(offset, target_offset, available, urn.value))
    if available == 0 : break

    offset += available

outfd = open("/tmp/foobar.dd", "w")
while 1:
    data = fd.read(1024*1024)
    if not data: break

    outfd.write(data)

outfd.close()
fd.cache_return()
