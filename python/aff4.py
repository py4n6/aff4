#!/usr/bin/python

import unittest
import uuid
import urllib, os, re, struct, zlib

NAMESPACE = "aff4:"
VOLATILE_NS = "aff2volatile:"
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

#/** Image attributes */
AFF4_CHUNK_SIZE =NAMESPACE + "chunk_size"
AFF4_COMPRESSION =NAMESPACE + "compression"
AFF4_CHUNKS_IN_SEGMENT =NAMESPACE + "chunks_in_segment"
AFF4_DIRECTORY_OFFSET =VOLATILE_NS + "directory_offset"
AFF4_DIRTY       =VOLATILE_NS + "dirty"

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

def DEBUG(fmt, *args):
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

int_re = re.compile("(\d+)([kKmMgGs]?)")
def parse_int(string):
    m = int_re.match(string)
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
        return -1

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

    def close(self):
        self.fd.close()

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

class Property:
    """ A property is an attribute, value pair """
    def __init__(self, attribute, value):
        self.attribute = attribute
        self.value = value

    def __eq__(self, x):
        return x==self.attribute

    def __str__(self):
        return "     %s = %s\n" % (self.attribute, self.value)
    
class Resolver:
    """ The resolver is a central point for storing facts about the universe """
    def __init__(self):
        self.urn = {}
        self.read_cache = Store()
        self.write_cache = Store()

    def add(self, uri, attribute, value):
        DEBUG("Adding %s: %s=%s", uri, attribute, value);
        try:
            props = self.urn[uri]
        except KeyError:
            props = []

        props.append(Property(attribute, value))
        self.urn[uri] = props
        
    def resolve(self, uri, attribute):
        try:
            for x in self.urn[uri]:
                if x == attribute:
                    return x.value
        except KeyError:
            return NoneObject("Unable to resolve uri")
        
    def create(self, class_reference):
        return class_reference(None, 'w')

    def open(self, uri, mode='r'):
        """ Opens the uri returning the requested object """
        ## Do we know what type it is?
        type = self.resolve(uri, AFF4_TYPE)
        if type:
            for scheme, prefix, handler in DISPATCH:
                if prefix == type:
                    return handler(uri, mode)
        
        ## find the handler according to the scheme:
        for scheme, prefix, handler in DISPATCH:
            if scheme and uri.startswith(prefix):
                return handler(uri, mode)

        ## We dont know how to open it.
        return NoneObject("Dont know how to handle URN %s. (type %s)" % (uri, type))

    def __str__(self):
        result = ''
        for urn in self.urn.keys():
            result += "******************** %s *****************\n" % urn
            for attr in self.urn[urn]:
                result += attr.__str__()

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
        self.compress_size = oracle.resolve(self.urn, AFF4_VOLATILE_COMPRESSED_SIZE)
        self.size = oracle.resolve(self.urn, AFF4_SIZE)

    def read(self, length=None):
        if length is None:
            length = self.size

        length = min(self.size - self.readptr, length)

        if self.compression == zipfile.ZIP_STORED:
            fd = oracle.open(self.backing_fd,'r')
            fd.seek(self.base_offset + self.readptr)
            result = fd.read(length)
            self.readptr += len(result)

            return result

class Image(FileLikeObject):
    """ A Image stores a large, seekable, compressed, contiguous, block of data.

    We do this by splitting the data into chunks, each chunk is stored
    in a bevy. A bevy is a segment which holds a given number of
    chunks back to back. The offset of each chunk within the bevy is
    given by the index segment.
    """
    def __init__(self, urn=None, mode='r'):
        FileLikeObject.__init__(self, urn, mode)

        self.chunk_size = parse_int(oracle.resolve(self.urn, AFF4_CHUNK_SIZE)) or 32*1024
        self.chunks_in_segment = parse_int(oracle.resolve(self.urn, AFF4_CHUNKS_IN_SEGMENT)) or 2048
        self.compression = parse_int(oracle.resolve(self.urn, AFF4_COMPRESSION)) or 8

        self.chunk_cache = Store()
    
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

            fd = oracle.open(bevy_urn, 'r')
            fd.seek(offset)
            chunk = zlib.decompress(fd.read(length))

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

            self.fp = oracle.open(stored, mode)
            self.load_from(stored)
        else:
            AFFObject.__init__(self, urn, mode)
            
    def finish(self):
        self.__init__(self.urn, self.mode)

    def load_from(self, filename):
        """ Tries to open the filename as a ZipVolume """

        fileobj = oracle.open(filename, 'r')

        ## We parse out the CD of each file and build an index
        zf = zipfile.ZipFile(fileobj, mode='r', allowZip64=True)
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

    def test_02_ZipVolume(self):
        """ Tests the ZipVolume implementation """
        filename = "file:///tmp/test.zip"
        z = ZipVolume(None, 'r')
        z.load_from(filename)

        ## Test the stream implementation
        fd = oracle.open("%s/properties" % z.urn)
        data = z.zf.read("properties")
        self.assertEqual(data,fd.read(1024))

        stream = oracle.open("urn:aff4:91abef2e-1f8b-4c4b-bada-2cd744696be3")
        fd = open("output.dd")
        while 1:
            data = stream.read(1024)
            data2 = fd.read(1024)
            if not data or not data2: break

            self.assertEqual(data2, data)
            
if __name__ == "__main__":
    unittest.main()
