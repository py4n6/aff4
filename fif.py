import zipfile, struct, zlib, re
import Queue, threading, time, binascii
import bisect, os

## The version of this implementation of FIF
VERSION = "FIF 1.0"

## Alas python zlib grabs the GIL so threads dont give much. This
## should be fixed soon: http://bugs.python.org/issue4738
## For now we disable threads
NUMBER_OF_THREAD = 2

class Store:
    """ A limited cache implementation """
    def __init__(self, limit=50):
        self.limit = limit
        self.cache = {}
        self.seq = []
        self.size = 0
        self.expired = 0

    def put(self, key, obj):
        self.cache[key] = obj
        self.size += len(obj)
        
        self.seq.append(key)
        if len(self.seq) >= self.limit:
            key = self.seq.pop(0)
            self.size -= len(self.cache[key])
            self.expired += 1
            del self.cache[key]

    def get(self, key):
        return self.cache[key]

class FIFFD:
    """ A baseclass to facilitate access to FIF Files"""
    def write_properties(self, target):
        """ This returns a formatted properties string. Properties are
        related to the stream. target is a zipfile.
        """
        self.properties.update(dict(size = self.size,
                                    type = self.type,
                                    name = self.stream_name,))
        result = ''
        for k,v in self.properties.items():
            result+="%s=%s\n" % (k,v)

        ## Make the properties file
        filename = "%s/properties" % self.stream_name
        target.writestr(filename, result)

    def seek(self, pos, whence=0):
        if whence==0:
            self.readptr = pos
        elif whence==1:
            self.readptr += pos
        elif whence==2:
            self.readptr = self.size + pos

    def tell(self):
        return self.readptr

class FIFWriter(FIFFD):
    """ A stream writer for creating a new FIF archive """
    ## The type of a stream is used to find the right driver for it.
    type = "Image"
    size = 0
    
    def __init__(self, filename, mode='w', stream_name='data',
                 chunksize=32*1024, attributes=None, threading=False, fd=None):
        
        if fd:
            self.fd = fd
        else:
            self.fd = zipfile.ZipFile(filename, mode, zipfile.ZIP_DEFLATED, True)
            
        self.stream_name = stream_name
        self.map = []
        self.chunk_id = 0
        self.chunksize = chunksize
        self.fd.writestr("version", VERSION)
        self.threading = threading
        
        ## The following are mandatory properties:
        self.properties = dict(chunksize = self.chunksize,)
        if attributes:
            self.properties.update(attributes)

        if threading:
        ## New compression jobs get pushed here
            self.IN_QUEUE = Queue.Queue(NUMBER_OF_THREAD+1)
            
        ## Results get pushed here by the threads
            self.OUT_QUEUE = Queue.Queue(NUMBER_OF_THREAD+1)

        ## Start a couple of threads
            self.threads = [] 
            for i in range(NUMBER_OF_THREAD):
                x = threading.Thread(target = self.compress_chunk_thread)
                x.start()

    def write_properties(self, target):
        self.properties['count'] = self.chunk_id+1
        FIFFD.write_properties(self, target)
            
    def compress_chunk_thread(self):
        """ This function runs forever in a new thread performing the
        compression jobs.
        """
        while 1:
            data, name = self.IN_QUEUE.get(True)
            ## We need to quit when theres nothing
            if name==None: return

            zinfo = zipfile.ZipInfo(filename = name,
                                date_time=time.localtime(time.time())[:6])
            zinfo.file_size = len(data)
            zinfo.CRC = binascii.crc32(data)
            zinfo.compress_type = self.fd.compression
            if zinfo.compress_type == zipfile.ZIP_DEFLATED:
                co = zlib.compressobj(-1,
                                      zlib.DEFLATED, -15)
                data = co.compress(data) + co.flush()
                zinfo.compress_size = len(data)    # Compressed size
            else:
                zinfo.compress_size = zinfo.file_size

            try:
                self.OUT_QUEUE.put( (data, zinfo), True, 1)
            except Queue.Full:
                return

    def check_queues(self):
        """ A function which writes the compressed queues to the file
        """
        try:
            while 1:
                bytes, zinfo = self.OUT_QUEUE.get(False)
                
                zinfo.header_offset = self.fd.fp.tell()    # Start of header bytes
                self.fd._writecheck(zinfo)
                self.fd._didModify = True

                self.fd.fp.write(zinfo.FileHeader())
                self.fd.fp.write(bytes)
                self.fd.fp.flush()
                if zinfo.flag_bits & 0x08:
                    # Write CRC and file sizes after the file data
                    self.fd.fp.write(struct.pack("<lLL", zinfo.CRC, zinfo.compress_size,
                                                 zinfo.file_size))
                self.fd.filelist.append(zinfo)
                self.fd.NameToInfo[zinfo.filename] = zinfo
                
        except Queue.Empty:
            pass
        
    def write_chunk(self, data):
        """ Adds the chunk to the archive """
        assert(len(data)==self.chunksize)
        name = "%s/%08d.dd" % (self.stream_name, self.chunk_id)
        if self.threading:
            self.IN_QUEUE.put( (data, name) )
            self.check_queues()
        else:
            self.fd.writestr(name, data)
            self.chunk_id += 1

        self.size += self.chunksize
        
    def close(self):
        """ Finalise the archive """
        if self.threading:
            ## Kill off the threads
            for i in range(NUMBER_OF_THREAD+1):
                self.IN_QUEUE.put((None, None))

            self.check_queues()
                
        self.write_properties(self.fd)        
        self.fd.close()

class FIFReader(FIFFD):
    """ A File like object which reads a FIF file """
    def __init__(self, fd, properties, stream_name='data'):
        self.fd = fd
        self.fp = fd.fp
        self.stream_name = stream_name
        self.readptr = 0
        self.properties = properties
        
        ## retrieve the mandatory properties
        self.chunksize = int(self.properties['chunksize'])
        self.size = int(self.properties['size'])
        self.Store = Store()
        self.stream_re = re.compile(r"%s/(\d+)+.dd" % self.stream_name)
        self.build_index()

    def build_index(self):
        """ This function parses all the chunks in the stream and
        builds an index directly into the file - this saves time.
        """
        self.index = {}
        for zinfo in self.fd.infolist():
            m = self.stream_re.match(zinfo.filename)
            if not m: continue
            
            self.fp.seek(zinfo.header_offset, 0)

            # Skip the file header:
            fheader = self.fp.read(30)
            if fheader[0:4] != zipfile.stringFileHeader:
                raise zipfile.BadZipfile, "Bad magic number for file header"

            fheader = struct.unpack(zipfile.structFileHeader, fheader)
            self.index[int(m.group(1))] = ( self.fp.tell() + \
                                            fheader[zipfile._FH_FILENAME_LENGTH] + \
                                            fheader[zipfile._FH_EXTRA_FIELD_LENGTH],
                                            zinfo.compress_size )
            
    def read(self, length):
        result = ''
        length = min(self.size - self.readptr, length)
        while length>0:
            data= self.partial_read(length)
            length -= len(data)
            result += data

        return result
    
    def partial_read(self, length):
        ## which chunk is it?
        chunk_id = self.readptr / self.chunksize
        chunk_offset = self.readptr % self.chunksize
        available_to_read = min(self.chunksize - chunk_offset, length)

        try:
            chunk = self.Store.get(chunk_id)
        except KeyError:
            ## Use the index to locate the data
            offset, length = self.index[chunk_id]
            self.fp.seek(offset)
            data = self.fp.read(length)

            dc = zlib.decompressobj(-15)
            chunk = dc.decompress(data)
            chunk += dc.decompress('Z') + dc.flush()
            self.Store.put(chunk_id, chunk)

        self.readptr += available_to_read
        return chunk[chunk_offset:chunk_offset+available_to_read]

    def stats(self):
        print "Store load %s chunks total %s (expired %s)" % (
            len(self.Store.seq),
            self.Store.size,
            self.Store.expired,
            )

class MapDriver(FIFFD):
    """ A Map driver is a read through mapping transformation of the
    target stream to create a new stream.

    We require the stream properties to specify a 'target'. This can
    either be a plain stream name or can begin with 'file://'. In the
    latter case this indicates that we should be opening an external
    file of the specified filename.

    We expect to find a component in the archive called 'map' which
    contains a mapping function. The file should be of the format:

    - lines starting with # are ignored
    
    - other lines have 2 integers seperated by white space. The first
    column is the current stream offset, while the second offset if
    the target stream offset.

    For example:
    0     1000
    1000  4000

    This means that when the current stream is accessed in the range
    0-1000 we fetch bytes 1000-2000 from the target stream, and after
    that we fetch bytes from offset 4000.

    Required properties:
    
    - target%d starts with 0 the number of target (may be specified as
      a URL). e.g. target0, target1, target2

    Optional properties:

    - file_period - number of bytes in the file offset which this map
      repreats on. (Useful for RAID)

    - image_period - number of bytes in the image each period will
      advance by. (Useful for RAID)
    
    """
    type = "Map"
    
    def __init__(self, zipfile, properties, stream_name):
        ## Build up the list of targets
        self.targets = {}
        try:
            count = 0
            while 1:
                self.targets[count] = open_stream(None, properties['target%d' % count],
                                                  fd = zipfile)
                count +=1
        except KeyError:
            pass

        if count==0:
            raise RuntimeError("You must set some targets of the mapping stream")

        ## Get the underlying target
        self.zipfile = zipfile
        
        ## This holds all the file offsets on the map in sorted order
        self.points = []

        ## This holds the image offsets corresponding to each file offset.
        self.mapping = {}

        ## This holds the target index for each file offset
        self.target_index = {}
        
        self.properties = properties
        self.stream_name = stream_name
        self.readptr = 0
        self.size = int(properties.get('size',0))

        ## Check if there is a map to load:
        try:
            self.load_map()
        except KeyError:
            pass
        
    def del_point(self, file_pos):
        """ Remove the point at file_pos if it exists """
        idx = self.points.index(file_pos)
        try:
            del self.mapping[file_pos]
            del self.target_index[file_pos]
            self.points.pop(idx)
        except:
            pass

    def add_point(self, file_pos, image_pos, target_index):
        """ Adds a new point to the mapping function. Points may be
        added in any order.
        """
        bisect.insort_left(self.points, file_pos)
        ## Check to see if the new point is different than what is to
        ## be expected - this assists in compressing the mapping
        ## function because we never store points unnecessarily.
        self.mapping[file_pos] = image_pos
        self.target_index[file_pos] = target_index

    def pack(self):
        """ Rewrites the points array to represent the current map
        better
        """
        last_file_point = self.points.pop(0)
        result = [last_file_point]
        last_image_point = self.mapping[last_file_point]
        
        for point in self.points:
            ## interpolate
            interpolated = last_image_point + (point - last_file_point)
            last_file_point = point
            last_image_point = self.mapping[last_file_point]
            
            if interpolated != last_image_point:
                result.append(point)

        self.points = result

    def seek(self, offset, whence=0):
        if whence==0:
            self.readptr = offset
        elif whence==1:
            self.readptr += offset
        elif whence==2:
            self.readptr = self.size or self.points[-1]

    def interpolate(self, file_offset, direction_forward=True):
        """ Provides a tuple of (image_offset, valid length) for the
        file_offset provided. The valid length is the number of bytes
        until the next discontinuity.
        """
        try:
            file_period = int(self.properties['file_period'])
            image_period = int(self.properties['image_period'])
            period_number = file_offset / file_period
            file_offset = file_offset % file_period
        except KeyError:
            period_number = 0
            image_period = 0
            file_period = self.size
        ## We can't interpolate forward before the first point - must
        ## interpolate backwards.
        if file_offset < self.points[0]:
            direction_forward = False

        ## We can't interpolate backwards after the last point, we must
        ## interpolate forwards.
        elif file_offset > self.points[-1]:
            direction_forward = True

        if direction_forward:
            l = bisect.bisect_right(self.points, file_offset)-1
            try:
                left = self.points[l+1] - file_offset
            except:
                left = file_period - file_offset

            point = self.points[l]
            image_offset = self.mapping[point]+file_offset - point
        else:
            r = bisect.bisect_right(self.points, file_offset)

            point = self.points[r]
            
            image_offset =self.mapping[point] - (point - file_offset)
            left = point - file_offset
            
        return (image_offset + image_period * period_number, left, self.target_index[point])

    def tell(self):
        return self.readptr

    def read(self, length):
        result = ''
        ## Cant read beyond end of file
        length = min(self.size - self.readptr, length)
        
        while length>0:
            m, left, target_index = self.interpolate(self.readptr)
            ## Which target to read from?
            fd = self.targets[target_index]

            fd.seek(m,0)
            want_to_read = min(left, length)
            
            data = fd.read(want_to_read)
            if len(data)==0: break

            self.readptr += len(data)
            result += data
            length -= len(data)

        return result

    def save_map(self):
        """ Saves the map onto the fif file.
        """
        ## reopen the zip file in write mode
        z = zipfile.ZipFile(self.zipfile.filename, 'a', zipfile.ZIP_DEFLATED, True)

        result = ''
        for x in self.points:
            result += "%s %s %s\n" % (x, self.mapping[x], self.target_index[x])

        self.write_properties(z)
        filename = "%s/map" % self.stream_name
        z.writestr(filename, result)
        z.close()

    def load_map(self):
        """ Opens the mapfile and loads the points from it """
        filename = "%s/map" % self.stream_name
        result =self.zipfile.read(filename)

        for line in result.splitlines():
            line = line.strip()
            if line.startswith("#"): continue
            try:
                temp = re.split("[\t ]+", line, 2)
                off = temp[0]
                image_off = temp[1]
                target_index = temp[2]

                self.add_point(int(off), int(image_off), int(target_index))
            except (ValueError,IndexError),e:
                pass

    def plot(self, title='', filename=None, type='png'):
        """ A utility to plot the mapping function """
        max_size = self.size
        p = os.popen("gnuplot", "w")
        if filename:
            p.write("set term %s\n" % type)
            p.write("set output \"%s\"\n" % filename)
        else:
            p.write("set term x11\n")
            
        p.write('pl "-" title "%s" w l, "-" title "." w p ps 5\n' % title)

        for point in self.points:
            p.write("%s %s\n" % (point, self.mapping[point]))
            
        p.write("e\n")

        for i in self.points:
            p.write("%s %s\n" % (i,self.mapping[i]))

        p.write("e\n")
        if not filename:
            p.write("pause 10\n")
            
        p.flush()

        return p

## The following are the supported segment types
types = dict(Image=FIFReader, Map=MapDriver)

## This is a factory function for accessing streams within the fif
## file.
def open_stream(filename, stream_name='data', fd=None):
    """ This is the main entrypoint to the FIF library. Use this
    function to open a named stream for reading from the file.
    """
    if not fd:
        fd = zipfile.ZipFile(filename, 'r', zipfile.ZIP_DEFLATED, True)
        ## Read the version:
        version = fd.read("version")
        print "File version %s" % version

    ## Parse the properties
    properties = {}
    filename = "%s/properties" % stream_name
    for line in fd.read(filename).splitlines():
        k,v = line.split("=",1)
        properties[k] = v

    ## Return the correct driver
    try:
        driver = types[properties.get('type', 'Image')]
    except KeyError:
        raise RuntimeError("No driver found for stream type %s" % \
                           properties.get('type','Image'))

    return driver(fd, properties, stream_name)
