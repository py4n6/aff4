#!/usr/bin/env python
""" This is an implementation of the advanced imager described in the paper

Advanced Hash based Imaging using AFF4

Submitted to the DFRWS 2010 conference.
"""
import hashlib
import pyaff4, os, sys
import pdb
import urllib
import threading
import pytsk3

import time
#time.sleep(1)

## This is a list of files we should not bother compressing
## because they will not be very compressible. For now we use
## filename extension to make this call
DO_NOT_COMPRESS = set("zip cab jpg avi mpg jar tgz msi swf mp3 mp4 wma".split())
FILENAME_URN = 'http://afflib.org/2009/aff4#hash_filename'

out_urn = pyaff4.RDFURN("/tmp/test3.aff4")

## Offset into the image where the filesystem can be found
OFFSET = 0
THREADS = 0

IN_FILENAME = "/var/tmp/uploads/testimages/winxp.E01"
IN_FILENAME = "/var/tmp/uploads/testimages/winxp.dd"
IN_FILENAME = "/var/tmp/uploads/testimages/ntfs_image.dd"
#IN_FILENAME = "/tmp/image.dd"

CORPUS_FILENAME = "/tmp/corpus.aff4"
CORPUS_TDB = pyaff4.TDB(CORPUS_FILENAME)

in_urn = pyaff4.RDFURN(IN_FILENAME)
ZERO_URN = pyaff4.RDFURN("aff4://zero")

oracle = pyaff4.Resolver()

class AFF4ImgInfo(pytsk3.Img_Info):
    def __init__(self, url):
        self.fd = oracle.open(url)
        if not self.fd:
            raise IOError("Unable to open %s" % url)

        pytsk3.Img_Info.__init__(self, '')

    def get_size(self):
        return self.fd.size.value

    def read(self, off, length):
        self.fd.seek(off)
        return self.fd.read(length)

    def close(self):
        self.fd.close()


def log(x, level=1):
    print "0x%lX: %s" % (threading.current_thread().ident & 0xFFFFFFFF, x)

def print_map(fd):
    offset = 0
    while offset < fd.size.value:
        urn, target_offset, available, target_id = fd.map.get_range(offset)
        log("%s,%s,%s,%s"%(offset, target_offset, available, urn.value))
        if available == 0 : break

        offset += available

class HashWorker(threading.Thread):
    ## The minimum size for a block to exist on its own. Smaller
    ## blocked will be dumped into the miscelaneous stream
    MIN_SIZE = 64 * 1024

    def __init__(self, in_urn, volume_urn,
                 image_stream_urn,
                 misc_stream_urn,
                 blocks, blocksize,
                 condition,
                 compression = pyaff4.ZIP_DEFLATE,
                 size = 0, attributes = None,
                 *args, **kwargs):
        threading.Thread.__init__(self, *args, **kwargs)

        self.in_urn = in_urn
        self.volume_urn = volume_urn
        self.blocks = blocks
        self.blocksize = blocksize
        self.image_stream_urn = image_stream_urn

        ## We signal this when we are done
        self.condition = condition
        self.inode = None
        self.compression = compression
        self.size = size
        self.attributes = attributes
        self.misc_stream_urn = misc_stream_urn

    def run(self):
        try:
            self.dump_blocks()
        finally:
            ## Signal the condition variable
            self.condition.acquire()
            self.condition.notify()
            self.condition.release()

    def dump_to_misc(self, data):
        """ Append the data to the misc stream instead of its own stream.

        This is done with streams which are too small to be put in
        their own segment.
        """
        fd = oracle.open(self.misc_stream_urn, 'w')
        try:
            end = fd.size.value
            fd.seek(end)

            fd.write(data)
        finally:
            fd.cache_return()

        ## Add the blocks to the image
        self.add_block_run(self.misc_stream_urn, file_offset = end)

    def check_corpus(self, hash):
        """ Returns true if the hash is already in the corpus """
        return bool(CORPUS_TDB.fetch(hash))

    def add_to_corpus(self, hash, name):
        CORPUS_TDB.store(hash, name)

    def dump_blocks(self):
        ## Read all the data into a single string
        data_blocks = []

        ## Open another read handle to the file private to our thread
        fd = oracle.open(self.in_urn, 'r')
        try:
            for block, length in self.blocks:
                fd.seek(block * self.blocksize)
                data_blocks.append(fd.read(length * self.blocksize))
        finally:
            fd.cache_return()

        data = ''.join(data_blocks)

        ## If there is not enough data here, we just write it to the
        ## miscelaneous stream
        if len(data) < self.MIN_SIZE:
            return self.dump_to_misc(data)

        ## Calculate the hash
        m = hashlib.sha1()
        m.update(data)

        hashed_urn = pyaff4.RDFURN("aff4://" + m.hexdigest())

        ## Check if the hashed file already exists
        if self.check_corpus(m.digest()):
            log("Skipping.... %s" % hashed_urn.value)
        else:
            compression = pyaff4.XSDInteger(self.compression)

            ## Save a copy of the data
            out_fd = oracle.create(pyaff4.AFF4_IMAGE)
            out_fd.urn.set(hashed_urn.value)
            out_fd.set(pyaff4.AFF4_STORED, self.volume_urn)
            out_fd.set(pyaff4.AFF4_COMPRESSION, compression)
            out_fd = out_fd.finish()
            out_fd.write(data)
            self.add_to_corpus(m.digest(), self.attributes.get(FILENAME_URN,'1'))

            ## Attach the attributes for this new object
            #pdb.set_trace()
            if self.attributes:
                for k,v in self.attributes.items():
                    try:
                        out_fd.set(k, v)
                    except:
                        out_fd.set(k, pyaff4.XSDString(str(v)))

            out_fd.close()


        if self.inode:
            string = pyaff4.XSDString(self.inode)
            log("Setting %s filename %s" % (hashed_urn.value, self.inode))
            oracle.add_value(hashed_urn, pyaff4.PREDICATE_NAMESPACE + "filename", string)

        self.add_block_run(hashed_urn)

    def add_block_run(self, hashed_urn, file_offset = 0):
        image_stream = oracle.open(self.image_stream_urn, 'w')

        try:
            for image_block, number_of_blocks in self.blocks:
                available = image_stream.map.add_point(image_block * self.blocksize,
                                                       file_offset,
                                                       hashed_urn.value)

                file_offset += number_of_blocks * self.blocksize

                ## the last block has some room after it - we need to add the
                ## Zero URN to it
                if available > number_of_blocks * self.blocksize:
                    image_stream.map.add_point((image_block + number_of_blocks) * self.blocksize,
                                               0, ZERO_URN.value)
        finally:
            image_stream.cache_return()


class HashImager:
    """ A class for creating a new hash based image.

    in_urn can be a file containing the volume, or an existing stream.
    """
    ## The total number of 512 byte sectors in each run
    MAX_SECTORS = 10000

    ## The minimum number of blocks in a single run. If files are less
    ## than this they will not be compressed in their own segments. If
    ## unallocated blocks are smaller than this they will all be
    ## merged into the miscelaneous stream.
    MIN_SIZE = 64

    MAX_THREADS = 4
    workers = []

    ## A condition variable for threads to sync on
    condition = threading.Condition()

    def __init__(self, in_urn, output_urn, *args, **kwargs):
        self.in_urn = in_urn

        ## Work out what the in_urn actually is. This code allows us
        ## to recompress an existing AFF4 volume.
        if oracle.load(in_urn):
            ## It contains a volume. We just get the first stream in
            ## the volume. FIXME - this should be refined a bit
            oracle.resolve_value(in_urn, pyaff4.AFF4_CONTAINS, in_urn)

        ## Check that the input urn is valid
        fd = oracle.open(in_urn, 'r')

        ## This is the final size of the image
        self.image_size = fd.size.value
        try:
            ## Make sure the object is a stream
            if not isinstance(fd, pyaff4.FileLikeObject):
                raise IOError("Input URN %s is not a stream" % in_urn.value)

            self.set_output_volume(output_urn)

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
            image_stream.size.set(self.image_size)
            image_stream.cache_return()

            ## Create a miscelaneous stream for small objects
            misc = oracle.create(pyaff4.AFF4_IMAGE)
            misc.urn.set(self.image_urn.value)
            misc.urn.add("misc")
            misc.set(pyaff4.AFF4_STORED, self.volume_urn)
            misc.finish()

            self.misc_urn = misc.urn

            misc.cache_return()

        finally:
            fd.cache_return()

    def set_output_volume(self, output_urn):
        """ Prepares an output volume for storing maps and streams based on the specified out_urn """
        ## Is the output urn already a volume? (if it is it will be updated to the volume URN)
        if oracle.load(output_urn):
            self.volume_urn = output_urn
        else:
            ## We need to make a new volume on it. FIXME - support encryption here
            volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
            volume.set(pyaff4.AFF4_STORED, output_urn)
            volume = volume.finish()
            self.volume_urn = volume.urn
            volume.cache_return()

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

            volume = oracle.open(self.volume_urn, 'w')
            volume.close()

        finally:
            self.fd.cache_return()

    def join_workers(self, wait=True):
        ## Wait for all the workers to finish
        current_workers = []
        for x in self.workers:
            if x.isAlive():
                current_workers.append(x)
            else:
                ## Make sure we join the threads so they can be cleaned up
                x.join()

        ## Only store the live threads
        self.workers = current_workers

        ## Hang around for a while or until one of the threads is
        ## finished
        if wait:
            self.condition.acquire()
            self.condition.wait(0.5)
            self.condition.release()

    def debug_map(self):
        fd = oracle.open(self.image_urn, 'w')
        print_map(fd)
        fd.cache_return()

    def image(self, imgoff=0):
        self.fd = oracle.open(self.in_urn, 'r')
        try:
            ## Create the image. First the filesystem
            self.dump_filesystem(imgoff)

            ## Wait for all the files to be written
            while len(self.workers)>0:
                self.join_workers()

            ## Now analyse the holes
            self.dump_unallocated()
            while len(self.workers)>0:
                self.join_workers()

            ## Done, close off all streams and output volume
            #log("Closing image_stream")
            image_stream = oracle.open(self.image_urn, 'w')
            image_stream.close()

            misc = oracle.open(self.misc_urn, 'w')
            misc.close()

            volume = oracle.open(self.volume_urn, 'w')
            volume.close()
        finally:
            self.fd.cache_return()

    def dump_block_run(self, blocks, inode=None,
                       compression = pyaff4.ZIP_DEFLATE,
                       size = 0, attributes = None):
        """ Dump the block run given as a reverse map """
        if not THREADS:
            ## we are operating in single thread mode
            worker = HashWorker(self.in_urn, self.volume_urn,
                                self.image_urn, self.misc_urn,
                                blocks,
                                self.blocksize,
                                self.condition,
                                compression = compression,
                                size = size,
                                attributes = attributes)
            return worker.run()

        while 1:
            ## Remove old threads from the pool
            self.join_workers(wait = False)

            #print self.workers
            ## If there is room for new threads make them
            if len(self.workers) < self.MAX_THREADS:
                ## Start a new worker
                worker = HashWorker(self.in_urn, self.volume_urn,
                                    self.image_urn, self.misc_urn,
                                    blocks,
                                    self.blocksize,
                                    self.condition,
                                    compression = compression,
                                    size = size,
                                    attributes = attributes)
                worker.inode = inode

                worker.start()
                #worker.join()
                self.workers.append(worker)
                #log( "spawned 0x%X" % (0xFFFFFFFF & worker.ident))
                return
            else:
                ## Wait a while for some threads to finish
                self.condition.acquire()
                self.condition.wait(0.5)
                self.condition.release()

    ## A mask of file types for which we do not produce block runs
    SKIPPED_FILE_FLAGS = pytsk3.TSK_FS_ATTR_RES | pytsk3.TSK_FS_ATTR_COMP | pytsk3.TSK_FS_ATTR_ENC
    def dump_directory(self, directory):
        for f in directory:
            ## Do not do hidden files or directories (stops us from
            ## recursing into .. dirs)
            filename = f.info.name.name
            if filename.startswith("."): continue

            ## If the directory member is really another directory,
            ## dump it as well. FIXME - limit recursion here.
            try:
                self.dump_directory(f.as_directory())
                continue
            except RuntimeError: pass

            ## Should we compress this file?
            compression = pyaff4.ZIP_DEFLATE
            try:
                fileext = filename.split(".")[-1].lower()
                if fileext in DO_NOT_COMPRESS:
                    compression = pyaff4.ZIP_STORED
            except: pass

            ## Cycle over all attributes and dump resident,
            ## non-compressed ones:
            try:
                inode = "%s" % f.info.meta.addr
            except: inode = "-"
            for attr in f:
                ## Dont compress resident or compressed files
                if int(attr.info.flags) & self.SKIPPED_FILE_FLAGS:
                    continue

                ## Dont bother with really short files
                attr_size = attr.info.size
                if attr_size < self.blocksize: continue

                log( "Dumping %s:%s" % (f.info.name.name, attr.info.name))
                block_run = []
                file_offset = 0

                try:
                    for run in attr:
                        ## Detect sparse files here
                        if run.offset != file_offset:
                            log("Sparse file detected", level=2)
                            raise IndexError("Sparse file detected, aborting")

                        block_run.append([run.addr, run.len])
                        file_offset += run.len

                    if file_offset * self.blocksize < attr_size:
                        log("Block run too short for file length", level=2)
                        continue

                    self.dump_full_run(block_run, inode, compression,
                                       size=attr.info.size,
                                       attributes = {
                            FILENAME_URN: filename}
                            )

                except IndexError: pass

    def dump_full_run(self, block_run, inode, compression, size, attributes = None):
        """ Given a full block run of a file, break it into smaller
        runs clamped at MAX_SIZE
        """
        current_length = 0
        current_run = []
        for offset, length in block_run:
            available = length

            ## This run will tip us over the MAX_SIZE - we therefore
            ## break it off into two smaller runs
            while current_length + length > self.MAX_SECTORS:
                available = self.MAX_SECTORS - current_length

                current_run.append((offset, available))
                offset += available
                length -= available
                self.dump_block_run(current_run, inode, compression,
                                    size = self.blocksize * self.MAX_SECTORS,
                                    attributes = attributes)
                size -= self.blocksize * self.MAX_SECTORS

                ## Start a new run
                current_length = 0
                current_run = []

            current_run.append((offset, available))

        ## Dump the run now
        self.dump_block_run(current_run, inode, compression = compression,
                            size = size, attributes = attributes)

    def dump_filesystem(self, imgoff):
        fd = AFF4ImgInfo(self.in_urn)
        fs = pytsk3.FS_Info(fd, offset = imgoff)
        self.blocksize = fs.info.block_size

        self.dump_directory(fs.open_dir("/"))

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
        ## Run over the entire map and look for ZERO segments - these
        ## will be dumped seperately
        offset = 0

        while offset < self.image_size:
            image_stream = oracle.open(self.image_urn, 'w')
            try:
                urn, target_offset, available, target_id = image_stream.map.get_range(offset)
            finally:
                image_stream.cache_return()

            if not available: break

            #log( "Checking map point %s,%s,%s,%s" % (offset, target_offset, available, urn.value))
            ## We only want gaps
            if urn.value == ZERO_URN.value:
                ## At this point the gaps should be even blocksizes.
                if (available % self.blocksize) != 0 or (offset % self.blocksize) != 0:
                    pdb.set_trace()
                    print "????"

                log( "Dumping unallocated from %s to %s (%s bytes)" % (
                        offset, offset+available, available))
                blocks = [[ offset / self.blocksize , available / self.blocksize ],]
                self.dump_full_run(blocks, None,
                                   pyaff4.ZIP_DEFLATE, size = available,
                                   attributes = {
                        FILENAME_URN: "Unallocated %s" % offset}
                                   )

            offset += available

## Shut up messages
class Renderer(pyaff4.Logger):
    def message(self, level, message):
        pass

#renderer = Renderer()
#oracle.register_logger(renderer)

imager = HashImager(in_urn, out_urn)
try:
    imager.image(OFFSET)
except Exception,e:
    print e
    pdb.post_mortem()
#imager.test()

fd = oracle.open(imager.image_urn, 'r')
log( "\nMap %s" % fd.urn.value)
log( "---------\n\n")
print_map(fd)

outfd = open("/tmp/foobar.dd", "w")
while 1:
    data = fd.read(1024*1024)
    if not data: break

    outfd.write(data)

outfd.close()
fd.cache_return()
