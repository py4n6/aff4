#!/usr/bin/python

import unittest
import uuid
import urllib, os, re, struct, zlib, time
import StringIO
from zipfile import ZIP_STORED, ZIP_DEFLATED, ZIP64_LIMIT, structCentralDir, stringCentralDir, ZIP_FILECOUNT_LIMIT, structEndArchive64, stringEndArchive64, structEndArchive, stringEndArchive
import threading, mutex


NAMESPACE = "aff4:"
VOLATILE_NS = "aff4volatile:"
FQN = "urn:" + NAMESPACE

#/** These are standard aff4 attributes */
AFF4_STORED     =NAMESPACE +"stored"
AFF4_TYPE       =NAMESPACE +"type"
AFF4_CONTAINS   =NAMESPACE +"contains"
AFF4_SIZE       =NAMESPACE +"size"
AFF4_SHA        =NAMESPACE +"sha256"
AFF4_TIMESTAMP  =NAMESPACE +"timestamp"

#/** ZipFile attributes */
AFF4_VOLATILE_HEADER_OFFSET   = VOLATILE_NS + "relative_offset_local_header"
AFF4_VOLATILE_COMPRESSED_SIZE = VOLATILE_NS + "compress_size"
AFF4_VOLATILE_CRC             = VOLATILE_NS + "crc32"
AFF4_VOLATILE_COMPRESSION     = VOLATILE_NS + "compression"
AFF4_VOLATILE_FILE_OFFSET     = VOLATILE_NS + "file_offset"
AFF4_VOLATILE_DIRTY           = VOLATILE_NS + "dirty"

#/** Image attributes */
AFF4_CHUNK_SIZE =NAMESPACE + "chunk_size"
AFF4_COMPRESSION =NAMESPACE + "compression"
AFF4_CHUNKS_IN_SEGMENT =NAMESPACE + "chunks_in_segment"
AFF4_DIRECTORY_OFFSET =VOLATILE_NS + "directory_offset"

#/** Link, encryption attributes */
AFF4_TARGET= NAMESPACE + "target"

#/** Map attributes */
AFF4_BLOCKSIZE= NAMESPACE + "blocksize"
AFF4_IMAGE_PERIOD= NAMESPACE + "image_period"
AFF4_TARGET_PERIOD= NAMESPACE + "target_period"

#/* Identity attributes */
AFF4_STATEMENT =NAMESPACE + "statement"
AFF4_CERT      =NAMESPACE + "x509"
AFF4_PRIV_KEY  =VOLATILE_NS + "priv_key"
AFF4_COMMON_NAME =NAMESPACE + "common_name"

#/** These are standard aff4 types */
AFF4_ZIP_VOLUME       ="zip_volume"
AFF4_DIRECTORY_VOLUME ="directory"
AFF4_SEGMENT          ="segment"
AFF4_LINK             ="link"
AFF4_IMAGE            ="image"
AFF4_MAP              ="map"
AFF4_ENCRYTED         ="encrypted"
AFF4_IDENTITY         ="identity"

#/** These are various properties */
AFF4_AUTOLOAD         =NAMESPACE +"autoload"

_DEBUG = 1
_INFO = 5
_WARNING = 10

VERBOSITY=_INFO

def DEBUG(verb, fmt, *args):
    if verb >= VERBOSITY:
        print fmt % args

import urllib
def escape_filename(filename):
    urllib.quote(filename)
    
def unescape_filename(filename):
    urllib.unquote(filename)

def fully_qualified_name(filename, context_name):
    if not filename.startswith(FQN):
        filename = "%s/%s" % (context_name, filename)

    return filename

def relative_name(filename, context_name):
    if filename.startswith(context_name):
        return filename[len(context_name)+1:]

    return filename

int_re = re.compile("(\d+)([kKmMgGs]?)")
def parse_int(string):
    """ Parses an integer from a string. Supports suffixes """
    try:
        m = int_re.match(string)
    except TypeError:
        return int(string)
    
    if not m: return NoneObject("string %r is not an integer" % string)

    base = int(m.group(1))
    suffix = m.group(2).lower()

    if not suffix:
        return base

    if suffix=='s':
        return base * 512

    if suffix == 'k':
        return base * 1024

    if suffix == 'm':
        return base * 1024 * 1024

    if suffix == 'g':
        return base * 1024 * 1024 * 1024

    return NoneObject("Unknown suffix '%r'" % suffix)

class NoneObject(object):
    """ A magical object which is like None but swallows bad
    dereferences, __getattribute__, iterators etc to return itself.

    Instantiate with the reason for the error.
    """
    def __init__(self, reason, strict=False):
        DEBUG(_DEBUG, "Error: %s" % reason)
        self.reason = reason
        self.strict = strict
        if strict:
            self.bt = get_bt_string()

    def __str__(self):
        ## If we are strict we blow up here
        if self.strict:
            result = "Error: %s\n%s" % (self.reason, self.bt)
            print result
            sys.exit(0)
        else:
            return "Error: %s" % (self.reason)

    ## Behave like an empty set
    def __iter__(self):
        return self

    def next(self):
        raise StopIteration()

    def __getattribute__(self,attr):
        try:
            return object.__getattribute__(self, attr)
        except AttributeError:
            return self

    def __bool__(self):
        return False

    def __nonzero__(self):
        return False

    def __eq__(self, other):
        return False

    ## Make us subscriptable obj[j]
    def __getitem__(self, item):
        return self

    def __add__(self, x):
        return self

    def __sub__(self, x):
        return self

    def __int__(self):
        return 0

    def __call__(self, *arg, **kwargs):
        return self
    

class AFFObject:
    """ All AFF4 objects extend this one """
    urn = None
    mode = 'r'
    
    def __init__(self, urn=None, mode='r'):
        self.urn = urn or "%s%s" % (FQN, uuid.uuid4())
        self.mode = mode
        
    def set_property(self, attribute, value):
        pass

    def finish(self):
        """ This method is called after the object is constructed and
        all properties are set.
        """
        pass

    def __str__(self):
        return "<%s: %s instance at 0x%X>" % (self.urn, self.__class__.__name__, hash(self))
        

class AFFVolume(AFFObject):
    """ A Volume simply stores segments """
    def load_from(self, urn):
        """ Load the volume from urn. urn must be a Stream Object """

class FileLikeObject(AFFObject):
    readptr = 0
    mode = 'r'
    data = None

    def __init__(self, urn=None, mode='r'):
        AFFObject.__init__(self, urn, mode)
        self.size = parse_int(oracle.resolve(self.urn, AFF4_SIZE)) or 0

    def seek(self, offset, whence=0):
        if whence == 0:
            self.readptr = offset
        elif whence == 1:
            self.readptr += offset
        elif whence == 2:
            self.readptr = self.size + offset

    def read(self, length):
        pass

    def write(self, data):
        pass

    def tell(self):
        return self.readptr

    def get_data(self):
        if not self.data:
            self.seek(0)
            self.data = self.read(self.size)
            
        return self.data

    def truncate(self, offset):
        pass

    def close(self):
        pass

def escape_filename(filename):
    return urllib.quote(filename)

def unescape_filename(filename):
    return urllib.unquote(filename)

class FileBackedObject(FileLikeObject):
    def __init__(self, uri, mode):
        if not uri.startswith("file://"):
            raise RuntimeError("You must have a fully qualified urn here, not %s" % urn)

        filename = uri[len("file://"):]
        escaped = escape_filename(filename)
        if mode == 'r':
            flags = 'rb'
        else:
            flags = 'ab'
            try:
                os.makedirs(os.path.dirname(escaped))
            except Exception,e:
                pass

        self.fd = open(escaped, mode)
        self.fd.seek(0,2)
        self.size = self.fd.tell()
        self.fd.seek(0)
        self.urn = uri

    def read(self, length=None):
        if length==None:
            length = self.size - self.readptr

        self.fd.seek(self.readptr)
        result = self.fd.read(length)
        self.readptr += len(result)
        return result

    def write(self, data):
        self.fd.seek(self.readptr)
        self.fd.write(data)
        self.readptr += len(data)

        return len(data)

    def close(self):
        self.fd.close()

    def flush(self):
        self.fd.flush()

class HTTPObject(FileLikeObject):
    pass

class Store:
    """ This is a cache which expires objects in oldest first manner. """
    def __init__(self, limit=5):
        self.age = []
        self.hash = {}
        self.limit = limit

    def expire(self):
        while len(self.age) > self.limit:
            x = self.age.pop(0)
            del self.hash[x]

    def add(self, urn, obj):
        self.hash[urn] = obj
        self.age.append(urn)

    def __getitem__(self, urn):
        return self.hash[urn]
    
class URNObject:
    """ A URNObject is an object in AFF4 space with a set of properties """
    def __init__(self, urn):
        self.properties = {}
        self.urn = urn
        ## FIXME handle locking here

    def __str__(self):
        result = ''
        for attribute,v in self.properties.items():
            for value in v:
                result += "       %s = %s\n" % (attribute, value)

        return result
    
    def add(self, attribute, value):
        try:
            properties = self.properties[attribute]
        except KeyError:
            properties = []

        if value not in properties:
            properties.append(value)
        self.properties[attribute] = properties

    def delete(self, attribute):
        """ Remove all attributes of these name from the object """
        self.properties[attribute] = []

    def set(self, attribute, value):
        """ Set the attribute with this value remove all previous settings """
        self.properties[attribute] = [value, ]

    def __getitem__(self, attribute):
        try:
            return self.properties[attribute]
        except KeyError:
            return NoneObject("URN %s has no attribute %s" % (self.urn, attribute))

    def export(self):
        result =''
        for attribute, v in self.properties.items():
            if not attribute.startswith(VOLATILE_NS):
                for value in v:
                    result += "%s=%s\r\n" % (attribute, value)
                
        return result

class Resolver:
    """ The resolver is a central point for storing facts about the universe """
    def __init__(self):
        self.urn = {}
        self.read_cache = Store()
        self.write_cache = Store()

    def __getitem__(self, urn):
        try:
            obj = self.urn[urn]
        except KeyError:
            obj = URNObject(urn)

        self.urn[urn] = obj
        return obj

    def set(self, uri, attribute, value):
        self[uri].set(attribute, value)
        
    def add(self, uri, attribute, value):
        DEBUG(_DEBUG, "Adding %s: %s=%s", uri, attribute, value);
        self[uri].add(attribute, value)

    def delete(self, uri, attribute):
        self[uri].delete(attribute)

    def resolve(self, uri, attribute):
        return self.resolve_list(uri,attribute)[0]
    
    def resolve_list(self, uri, attribute):
        try:
            return self[uri][attribute]
        except KeyError:
            return NoneObject("Unable to resolve uri")
        
    def create(self, class_reference):
        return class_reference(None, 'w')

    def cache_return(self, obj):
        DEBUG(_DEBUG, "Returning %s (%s)" % (obj.urn, obj.mode))
        if obj.mode == 'w':
            self.write_cache.add(obj.urn, obj)
        else:
            self.read_cache.add(obj.urn, obj)

    def open(self, uri, mode='r'):
        DEBUG(_DEBUG, "oracle: Openning %s (%s)" % (uri,mode))
        """ Opens the uri returning the requested object """
        try:
            if mode =='r':
                return self.read_cache[uri]
            else:
                return self.write_cache[uri]
        except KeyError:
            pass

        ## Do we know what type it is?
        type = self.resolve(uri, AFF4_TYPE)
        if type:
            for scheme, prefix, handler in DISPATCH:
                if prefix == type:
                    result = handler(uri, mode)
                    result.mode = mode
                    return result
        
        ## find the handler according to the scheme:
        for scheme, prefix, handler in DISPATCH:
            if scheme and uri.startswith(prefix):
                result = handler(uri, mode)
                result.mode = mode
                return result

        ## We dont know how to open it.
        return NoneObject("Dont know how to handle URN %s. (type %s)" % (uri, type))

    def __str__(self):
        result = ''
        for urn,obj in self.urn.items():
            result += "******************** %s *****************\n" % urn
            result += obj.__str__()

        return result

    full_props_re = re.compile("([^ ]+) ([^=]+)=(.+)")
    relative_props_re = re.compile("([^=]+)=(.+)")

    def parse_properties(self, data, context=None):
        """ Parses the properties given in the data and add to the resolver.

        Context is used if no explicit subject is given.
        """
        for line in data.splitlines():
            ## Is it a fully qualified line?
            m = self.full_props_re.match(line)
            if m:
                self.add(m.group(1), m.group(2), m.group(3))
                continue
            ## Or relative to the current context?
            m = self.relative_props_re.match(line)
            if m:
                self.add(context, m.group(1), m.group(2))
                continue

            print "Unknown line in properties: %s" % line

    def export(self, subject):
        return self[subject].export()
        
oracle = Resolver()

import zipfile

class ZipFileStream(FileLikeObject):
    """ This is a stream object which reads data from within a zip
    file. Note that the archive file is mapped and each read request
    is made from the backing fd, rather than reading and decompressing
    the whole thing at once - this makes it efficient to read very
    large bevies.
    """
    def __init__(self, urn=None, mode='r'):
        AFFObject.__init__(self, urn, mode)

        ## What volume are we in?
        self.volume = oracle.resolve(self.urn, AFF4_STORED)
        
        ## Where is it stored?
        self.backing_fd = oracle.resolve(self.volume, AFF4_STORED)

        ## our base offset
        self.base_offset = oracle.resolve(self.urn, AFF4_VOLATILE_FILE_OFFSET)
        self.compression = oracle.resolve(self.urn, AFF4_VOLATILE_COMPRESSION)
        self.compress_size = parse_int(oracle.resolve(self.urn, AFF4_VOLATILE_COMPRESSED_SIZE))
        self.size = parse_int(oracle.resolve(self.urn, AFF4_SIZE))

    def read(self, length=None):
        if length is None:
            length = self.size

        length = min(self.size - self.readptr, length)

        if self.compression == zipfile.ZIP_STORED:
            fd = oracle.open(self.backing_fd,'r')
            fd.seek(self.base_offset + self.readptr)
            result = fd.read(length)
            oracle.cache_return(fd)
            self.readptr += len(result)

            return result

class ImageWorker(threading.Thread):
    """ This is a worker responsible for creating a full bevy """
    def __init__(self, urn, volume, bevy_number, chunk_size, chunks_in_segment):
        threading.Thread.__init__(self)
        
        self.buffer = StringIO.StringIO()
        self.bevy = StringIO.StringIO()
        self.bevy_index = StringIO.StringIO()
        self.chunk_size = chunk_size
        self.chunks_in_segment = chunks_in_segment
        self.bevy_size = chunk_size * chunks_in_segment
        self.len = 0
        self.volume = volume
        self.urn = urn
        self.bevy_number = bevy_number
        
    def write(self, data):
        available_to_read = min(self.bevy_size - self.buffer.len, len(data))
        self.buffer.write(data[:available_to_read])
        
        return data[available_to_read:]

    def run(self):
        """ This is run when we have a complete bevy """
        print "Starting thread"
        self.buffer.seek(0)
        offset = 0
        while self.buffer.tell() < self.buffer.len:
            data = self.buffer.read(self.chunk_size)
            cdata = zlib.compress(data, 1)
            self.bevy.write(cdata)
            self.bevy_index.write(struct.pack("<L", offset))
            offset += len(cdata)

        self.bevy_index.write(struct.pack("<L", -1))

        ## Grab the volume
        volume = oracle.open(self.volume, 'w')

        ## Write the bevy
        volume.writestr(fully_qualified_name("%08d" % self.bevy_number, self.urn),
                        self.bevy.getvalue())
        volume.writestr(fully_qualified_name("%08d.idx" % self.bevy_number, self.urn),
                        self.bevy_index.getvalue())

        ## Done
        oracle.cache_return(volume)
        print "Finishing thread"
        
class Image(FileLikeObject):
    """ A Image stores a large, seekable, compressed, contiguous, block of data.

    We do this by splitting the data into chunks, each chunk is stored
    in a bevy. A bevy is a segment which holds a given number of
    chunks back to back. The offset of each chunk within the bevy is
    given by the index segment.
    """
    def __init__(self, urn=None, mode='r'):
        FileLikeObject.__init__(self, urn, mode)

        self.container = oracle.resolve(self.urn, AFF4_STORED)
        self.chunk_size = parse_int(oracle.resolve(self.urn, AFF4_CHUNK_SIZE)) or 32*1024
        self.chunks_in_segment = parse_int(oracle.resolve(self.urn, AFF4_CHUNKS_IN_SEGMENT)) or 2048
        self.compression = parse_int(oracle.resolve(self.urn, AFF4_COMPRESSION)) or 8

        self.chunk_cache = Store()
        self.bevy_number = 0
        self.running = []

        if mode=='w':
            self.writer = ImageWorker(self.urn, self.container, self.bevy_number,
                                      self.chunk_size, self.chunks_in_segment)

    def finish(self):
        oracle.set(self.urn, AFF4_TYPE, AFF4_IMAGE)
        self.__init__(self.urn, self.mode)

    def write(self, data):
        self.readptr += len(data)
        oracle.set(self.urn, AFF4_SIZE, self.readptr)
        while 1:
            data = self.writer.write(data)

            ## All the data fit:
            if not data:
                return

            ## It didnt all fit - flush the worker, and get a new one here
            self.writer.start()
            self.running.append(self.writer)
            print self.running
            
            ## Update our view of the currently running threads
            while 1:
                ## This can be written nicer using condition vars
                self.running = [ x for x in self.running if x.is_alive() ]
                if len(self.running) < 2:
                    break
                
                time.sleep(1)
                    
            self.bevy_number += 1
            self.writer = ImageWorker(self.urn, self.container, self.bevy_number,
                                      self.chunk_size, self.chunks_in_segment)

    def close(self):
        self.running.append(self.writer)
        self.writer.start()

        ## Wait untill all workers are finished
        for x in self.running:
            x.join()

    def partial_read(self, length):
        """ Read as much as is possible at the current point without
        exceeding a chunk
        """
        chunk_id = self.readptr / self.chunk_size
        chuck_offset = self.readptr % self.chunk_size
        available_to_read = min(self.chunk_size - chuck_offset, length)

        try:
            chunk = self.chunk_cache[chunk_id]
        except KeyError:
            bevy = chunk_id / self.chunks_in_segment
            chunk_in_bevy = chunk_id % self.chunks_in_segment
            
            bevy_urn = "%s/%08d" % (self.urn, bevy)
            
            fd = oracle.open("%s.idx" % bevy_urn, 'r')
            data = fd.get_data()
            offset, length = struct.unpack("<LL", data[4 * chunk_in_bevy:4 * chunk_in_bevy + 8])
            oracle.cache_return(fd)

            fd = oracle.open(bevy_urn, 'r')
            fd.seek(offset)
            chunk = zlib.decompress(fd.read(length))
            oracle.cache_return(fd)
            
            self.chunk_cache.add(chunk_id, chunk)
            
        return chunk[chuck_offset: chuck_offset + available_to_read]
        
    def read(self, length=None):        
        if length is None:
            length = self.size

        length = min(self.size - self.readptr, length)

        result = ''
        while length > 0:
            data = self.partial_read(length)
            if len(data)==0:
                break

            self.readptr += len(data)
            result += data
            length -= len(data)

        return result

class ZipVolume(AFFVolume):
    """ AFF4 Zip Volumes store segments within zip files """
    def __init__(self, urn, mode):
        if urn:
            stored = oracle.resolve(urn, AFF4_STORED)
            if not stored:
                raise RuntimeError("Can not find storage for Volumes %s" % urn)

            try:
                self.load_from(stored)
            except IOError: pass
        else:
            AFFObject.__init__(self, urn, mode)
            
    def finish(self):
        oracle.set(self.urn, AFF4_TYPE, AFF4_ZIP_VOLUME)
        self.__init__(self.urn, self.mode)

    def writestr(self, subject, data, compress_type = ZIP_STORED):
        """ Write the filename on the archive.

        subject is a fully qualified name or relative to the current volume.
        """
        DEBUG(_INFO, "Writing segment %s" % subject)
        filename = escape_filename(relative_name(subject, self.urn))

        ## Where are we stored?
        stored = oracle.resolve(self.urn, AFF4_STORED)

        ## This locks the backing_fd for exclusive access
        backing_fd = oracle.open(stored,'w')

        ## Mark the file as dirty
        oracle.set(self.urn, AFF4_VOLATILE_DIRTY, '1')

        ## Where should we write the new file?
        directory_offset = parse_int(oracle.resolve(self.urn, AFF4_DIRECTORY_OFFSET)) or 0

        zinfo = zipfile.ZipInfo(filename = filename,
                             date_time=time.localtime(time.time())[:6])
        
        zinfo.external_attr = 0600 << 16L      # Unix attributes        
        zinfo.header_offset = directory_offset
        zinfo.file_size = len(data)
        zinfo.CRC = zipfile.crc32(data)

        ## Compress the data if needed
        zinfo.compress_type = compress_type
        if zinfo.compress_type == ZIP_DEFLATED:
            co = zlib.compressobj(zlib.Z_DEFAULT_COMPRESSION,
                 zlib.DEFLATED, -15)
            data = co.compress(data) + co.flush()
            zinfo.compress_size = len(data)    # Compressed size
        else:
            zinfo.compress_size = zinfo.file_size

        ## Write the header and data
        backing_fd.seek(directory_offset)
        backing_fd.write(zinfo.FileHeader())
        backing_fd.write(data)

        ## Adjust the new directory_offset
        oracle.set(self.urn, AFF4_DIRECTORY_OFFSET, backing_fd.tell())

        ## Add this new file to the resolver
        subject = fully_qualified_name(subject, self.urn)
        self.import_zinfo(subject, zinfo)

        ## Done with the backing file now (this will unlock it too)
        oracle.cache_return(backing_fd)

    def close(self):
        """ Close and write a central directory structure on this zip file.
        
        This code was adapted from python2.6's zipfile.ZipFile.close()
        """
        ## Is this file dirty?
        if oracle.resolve(self.urn, AFF4_VOLATILE_DIRTY):
            ## Store volume properties
            self.writestr("properties", oracle.export(self.urn))

            ## Where are we stored?
            stored = oracle.resolve(self.urn, AFF4_STORED)
            
            ## This locks the backing_fd for exclusive access
            backing_fd = oracle.open(stored,'w')

            ## Get all the files we own
            count = 0
            extra = []
            pos1 = oracle.resolve(self.urn, AFF4_DIRECTORY_OFFSET)
            backing_fd.seek(pos1)
            
            for subject in oracle.resolve_list(self.urn, AFF4_CONTAINS):
                filename = escape_filename(relative_name(subject, self.urn))
                zinfo = zipfile.ZipInfo(filename)

                zinfo.header_offset = oracle.resolve(subject, AFF4_VOLATILE_HEADER_OFFSET)
                zinfo.compress_size = oracle.resolve(subject, AFF4_VOLATILE_COMPRESSED_SIZE)
                zinfo.file_size = oracle.resolve(subject, AFF4_SIZE)
                zinfo.compress_type = oracle.resolve(subject, AFF4_VOLATILE_COMPRESSION)
                zinfo.CRC = oracle.resolve(subject, AFF4_VOLATILE_CRC)
                zinfo.date_time = oracle.resolve(subject, AFF4_TIMESTAMP) or \
                                  time.localtime(time.time())[:6]
                
                count = count + 1
                dt = zinfo.date_time
                dosdate = (dt[0] - 1980) << 9 | dt[1] << 5 | dt[2]
                dostime = dt[3] << 11 | dt[4] << 5 | (dt[5] // 2)
                extra = []
                if zinfo.file_size > ZIP64_LIMIT \
                        or zinfo.compress_size > ZIP64_LIMIT:
                    extra.append(zinfo.file_size)
                    extra.append(zinfo.compress_size)
                    file_size = 0xffffffff
                    compress_size = 0xffffffff
                else:
                    file_size = zinfo.file_size
                    compress_size = zinfo.compress_size

                if zinfo.header_offset > ZIP64_LIMIT:
                    extra.append(zinfo.header_offset)
                    header_offset = 0xffffffffL
                else:
                    header_offset = zinfo.header_offset

                extra_data = zinfo.extra
                if extra:
                    # Append a ZIP64 field to the extra's
                    extra_data = struct.pack(
                            '<HH' + 'Q'*len(extra),
                            1, 8*len(extra), *extra) + extra_data

                    extract_version = max(45, zinfo.extract_version)
                    create_version = max(45, zinfo.create_version)
                else:
                    extract_version = zinfo.extract_version
                    create_version = zinfo.create_version

                try:
                    filename, flag_bits = zinfo._encodeFilenameFlags()
                    centdir = struct.pack(structCentralDir,
                     stringCentralDir, create_version,
                     zinfo.create_system, extract_version, zinfo.reserved,
                     flag_bits, zinfo.compress_type, dostime, dosdate,
                     zinfo.CRC, compress_size, file_size,
                     len(filename), len(extra_data), len(zinfo.comment),
                     0, zinfo.internal_attr, zinfo.external_attr,
                     header_offset)
                except DeprecationWarning:
                    print >>sys.stderr, (structCentralDir,
                     stringCentralDir, create_version,
                     zinfo.create_system, extract_version, zinfo.reserved,
                     zinfo.flag_bits, zinfo.compress_type, dostime, dosdate,
                     zinfo.CRC, compress_size, file_size,
                     len(zinfo.filename), len(extra_data), len(zinfo.comment),
                     0, zinfo.internal_attr, zinfo.external_attr,
                     header_offset)
                    raise
                backing_fd.write(centdir)
                backing_fd.write(filename)
                backing_fd.write(extra_data)
                backing_fd.write(zinfo.comment)

            pos2 = backing_fd.tell()
            # Write end-of-zip-archive record
            centDirCount = count
            centDirSize = pos2 - pos1
            centDirOffset = pos1
            if (centDirCount >= ZIP_FILECOUNT_LIMIT or
                centDirOffset > ZIP64_LIMIT or
                centDirSize > ZIP64_LIMIT):
                # Need to write the ZIP64 end-of-archive records
                zip64endrec = struct.pack(
                        structEndArchive64, stringEndArchive64,
                        44, 45, 45, 0, 0, centDirCount, centDirCount,
                        centDirSize, centDirOffset)
                backing_fd.write(zip64endrec)

                zip64locrec = struct.pack(
                        structEndArchive64Locator,
                        stringEndArchive64Locator, 0, pos2, 1)
                backing_fd.write(zip64locrec)
                centDirCount = min(centDirCount, 0xFFFF)
                centDirSize = min(centDirSize, 0xFFFFFFFF)
                centDirOffset = min(centDirOffset, 0xFFFFFFFF)

            # check for valid comment length
            endrec = struct.pack(structEndArchive, stringEndArchive,
                                 0, 0, centDirCount, centDirCount,
                                 centDirSize, centDirOffset, len(self.urn))
            backing_fd.write(endrec)
            backing_fd.write(self.urn)
            backing_fd.flush()
            
            oracle.cache_return(backing_fd)
            oracle.delete(self.urn, AFF4_VOLATILE_DIRTY)
            

    def load_from(self, filename):
        """ Tries to open the filename as a ZipVolume """

        fileobj = oracle.open(filename, 'r')

        ## We parse out the CD of each file and build an index
        zf = zipfile.ZipFile(fileobj, mode='r', allowZip64=True)
        oracle.cache_return(fileobj)
        
        self.zf = zf
        
        ## Here we should have a valid zip file - what is our URN?
        try:
            self.urn = zf.comment
            assert(self.urn.startswith(FQN))
        except Exception,e:
            ## Nope - maybe its in a __URN__ member
            try:
                self.urn = zf.read("__URN__")
                assert(self.urn.startswith(FQN))
            except:
                print "Volume does not have a valid URN - using temporary URN %s" % self.urn
                
        oracle.add(self.urn, AFF4_STORED, filename)
        oracle.add(filename, AFF4_CONTAINS, self.urn)
        
        infolist = zf.infolist()
        for zinfo in infolist:
            subject = fully_qualified_name(unescape_filename(zinfo.filename), self.urn)

            if zinfo.filename.endswith("properties"):
                ## A properties file refers to the object which
                ## contains it:
                oracle.parse_properties(zf.read(zinfo.filename),
                                        context=os.path.dirname(subject))
                
            self.import_zinfo(subject, zinfo)
            
    def import_zinfo(self, subject, zinfo):
        """ Import all the info from a zinfo into the resolver """
        ## Add some stats to the resolver these can be used in
        ## place of zinfo in future:
        oracle.add(self.urn, AFF4_CONTAINS, subject)
        oracle.add(subject, AFF4_STORED, self.urn)
        oracle.add(subject, AFF4_TYPE, AFF4_SEGMENT)

        oracle.add(subject, AFF4_VOLATILE_HEADER_OFFSET, zinfo.header_offset)
        oracle.add(subject, AFF4_VOLATILE_CRC, zinfo.CRC)
        oracle.add(subject, AFF4_VOLATILE_COMPRESSED_SIZE, zinfo.compress_size)
        oracle.add(subject, AFF4_SIZE, zinfo.file_size)
        oracle.add(subject, AFF4_VOLATILE_COMPRESSION, zinfo.compress_type)
        oracle.set(subject, AFF4_TIMESTAMP, zinfo.date_time)

        ## We just store the actual offset where the file is
        ## so we dont need to keep calculating it all the time
        oracle.add(subject, AFF4_VOLATILE_FILE_OFFSET, zinfo.header_offset +
                   zipfile.sizeFileHeader + len(zinfo.filename))
        
## This is a dispatch table for all handlers of a URL method. The
## format is scheme, URI prefix, handler class.
DISPATCH = [
    [ 1, "file://", FileBackedObject ],
    [ 0, AFF4_SEGMENT, ZipFileStream ],
    [ 0, AFF4_IMAGE, Image ],
    [ 0, AFF4_ZIP_VOLUME, ZipVolume ],
    ]

            
class ResolverTests(unittest.TestCase):
    def test_00access(self):
        location = "file://somewhere"
        test_urn = "urn:aff4:testurn"
        test_attr = "aff4:stored"
        oracle.add(test_urn, test_attr, location)
        self.assertEqual( location, oracle.resolve(test_urn, test_attr))

    def test_01FileBackedObject(self):
        """ Tests the file backed object """
        filename = "file://tests/FileBackedObject_test.txt"
        test_string = "hello world"
        
        fd = FileBackedObject(filename, 'w')
        fd.write(test_string)
        fd.close()
        
        fd = oracle.open(filename,'r')
        self.assertEqual(fd.read(), test_string)
        oracle.cache_return(fd)

    def xtest_02_ZipVolume(self):
        """ Test creating of zip files """
        filename = "file://tests/test.zip"

        out_fd = ZipVolume(None, "w")
        oracle.add(out_fd.urn, AFF4_STORED, filename)
        out_fd.finish()

        out_fd.writestr("default","Hello world")
        out_fd.close()

    def test_02_ImageWriting(self):
        """ Makes a new volume """
        ResolverTests.filename = filename = "file://tests/test.zip"
        in_filename = "file://output.dd"

        ## Make a new volume
        volume_fd = ZipVolume(None, "w")
        volume_urn = volume_fd.urn
        oracle.add(volume_urn, AFF4_STORED, filename)
        volume_fd.finish()
        oracle.cache_return(volume_fd)

        ## Make an image
        image_fd = Image(None, "w")
        oracle.add(image_fd.urn, AFF4_STORED, volume_urn)
        ResolverTests.image_urn = image_fd.urn
        image_fd.finish()
        in_fd = oracle.open(in_filename)

        while 1:
            data = in_fd.read(10240)
            if not data: break

            image_fd.write(data)

        image_fd.close()
        oracle.cache_return(in_fd)
        
        volume_fd = oracle.open(volume_urn, 'w')
        volume_fd.close()

    def test_03_ZipVolume(self):
        """ Tests the ZipVolume implementation """
        z = ZipVolume(None, 'r')
        z.load_from(ResolverTests.filename)

        ## Test the stream implementation
        fd = oracle.open("%s/properties" % z.urn)
        data = z.zf.read("properties")
        self.assertEqual(data,fd.read(1024))
        oracle.cache_return(fd)
        stream = oracle.open(ResolverTests.image_urn)
        fd = open("output.dd")
        while 1:
            data = stream.read(1024)
            data2 = fd.read(1024)
            if not data or not data2: break

            self.assertEqual(data2, data)

        import sk
        stream.seek(0)
        fs = sk.skfs(stream)

        print fs.listdir('/')
                    
if __name__ == "__main__":
    unittest.main()
