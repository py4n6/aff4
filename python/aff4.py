#!/usr/bin/python

import unittest
import uuid, posixpath, hashlib
import urllib, os, re, struct, zlib, time, sys
import StringIO
from zipfile import ZIP_STORED, ZIP_DEFLATED, ZIP64_LIMIT, structCentralDir, stringCentralDir, structEndArchive64, stringEndArchive64, structEndArchive, stringEndArchive, structFileHeader
import threading, mutex

ZIP_FILECOUNT_LIMIT = 1<<16
sizeFileHeader = struct.calcsize(structFileHeader)

## namespaces
NAMESPACE = "aff4:" ## AFF4 namespace
VOLATILE_NS = "aff4volatile:" ## Never written to files
CONFIGURATION_NS = VOLATILE_NS + "config:" ## Used to configure the
                                           ## library with global
                                           ## settings
FQN = "urn:" + NAMESPACE

GLOBAL = VOLATILE_NS + "global" ## A special urn representing the
                                ## global context

#Configuration parameters
CONFIG_THREADS = CONFIGURATION_NS + "threads"
CONFIG_VERBOSE = CONFIGURATION_NS + "verbosity"

#/** These are standard aff4 attributes */
AFF4_STORED     =NAMESPACE +"stored"
AFF4_TYPE       =NAMESPACE +"type"
AFF4_INTERFACE  =NAMESPACE +"interface"
AFF4_CONTAINS   =NAMESPACE +"contains"
AFF4_SIZE       =NAMESPACE +"size"
AFF4_SHA        =NAMESPACE +"sha256"
AFF4_TIMESTAMP  =NAMESPACE +"timestamp"

## Supported interfaces
AFF4_STREAM     =NAMESPACE +"stream"
AFF4_VOLUME     =NAMESPACE +"volume"

#/** ZipFile attributes */
AFF4_VOLATILE_HEADER_OFFSET   = VOLATILE_NS + "relative_offset_local_header"
AFF4_VOLATILE_COMPRESSED_SIZE = VOLATILE_NS + "compress_size"
AFF4_VOLATILE_CRC             = VOLATILE_NS + "crc32"
AFF4_VOLATILE_COMPRESSION     = VOLATILE_NS + "compression"
AFF4_VOLATILE_FILE_OFFSET     = VOLATILE_NS + "file_offset"
AFF4_VOLATILE_DIRTY           = VOLATILE_NS + "dirty"

## Volume attributes
AFF4_IDENTITY_STORED = NAMESPACE + "identity" ## Indicates an identity
                                              ## is stored in this
                                              ## volume

AFF4_AUTOLOAD = NAMESPACE +"autoload" ## A hint that this stream
                                      ## should be automatically
                                      ## loaded as a volume

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
AFF4_IDENTITY_PREFIX  =FQN + "identity"

#/** These are standard aff4 types */
AFF4_ZIP_VOLUME       ="zip_volume"
AFF4_DIRECTORY_VOLUME ="directory"
AFF4_SEGMENT          ="segment"
AFF4_LINK             ="link"
AFF4_IMAGE            ="image"
AFF4_MAP              ="map"
AFF4_ENCRYTED         ="encrypted"
AFF4_IDENTITY         ="identity"

## Some verbosity settings
_DEBUG = 10
_INFO = 5
_WARNING = 1

def DEBUG(verb, fmt, *args):
    pass

import urllib
def escape_filename(filename):
    urllib.quote(filename)
    
def unescape_filename(filename):
    urllib.unquote(filename)

def fully_qualified_name(filename, context_name):
    if not filename.startswith(FQN):
        filename = "%s/%s" % (context_name, filename)

    return posixpath.normpath(filename)

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

def Raise(reason):
    raise RuntimeError(reason)

class AFFObject:
    """ All AFF4 objects extend this one.

    Object protocol
    ===============

    AFFObjects are created via two mechanisms:

    1) The oracle.open(urn) method is called with the explicit
    URN. This will call the constructor and pass this URN. The object
    is then expected to initialise using various attributes of the URN
    from the universal resolver (e.g. AFF4_STORED to find its backing
    object etc).

    2) When an object is created for the first time (and does not
    already exist in the resolver) it does not have a known URN. In
    this case the following protocol is followed:

       a) Create a new instancd of the object - by passing None to the
       URN it is expected to generate a new valid URN:

          obj = AFFObjects() 

       b) Now obj.urn is a new valid and unique URN. We set all
       required properties:

          oracle.set(obj.urn, AFF4_STORED, somewhere)
          oracle.set(obj.urn, AFF4_CERT, ...)
          etc.

       c) Now call the finish method of the object to complete
       it. This method should check that all required parameters were
       provided, set defaults etc.

          obj.finish()


       Once the finish() method is called the object is complete and
       is the same as that obtained from method 1 above.


    Thread protocol
    ===============

    AFFObjects are always managed via the universal resolver. This
    means that they must be returned to the resolver after we finish
    using them. Obtaining an object via oracle.open() actually locks
    the object so other threads can not acquire it until it is
    returned to the oracle. Failing to return the object will result
    in thread deadlocks.

    For example, the following pattern must be used:

    fd = oracle.open(urn, 'r')
    try:
        ## do stuff with the fd
    finally:
        oracle.cache_return(fd)

    Your thread will hold a lock on the object between the try: and
    finally. You must not touch the object once its returned to the
    oracle because it may be in use by some other thread (although
    fd.urn is not going to change so its probably ok to read it). You
    must ensure that the object is returned to the cache as soon as
    possible to avoid other threads from blocking too long.
    """
    urn = None
    mode = 'r'
    
    def __init__(self, urn=None, mode='r'):
        self.urn = urn or "%s%s" % (FQN, uuid.uuid4())
        self.mode = mode
        
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
    """ Escape a URN so its suitable to be stored in filesystems.

    Windows has serious limitations of the characters allowed in
    filenames, so we need to escape them whenever we store segments in
    files.
    """
    return urllib.quote(filename)

def unescape_filename(filename):
    return urllib.unquote(filename)

class FileBackedObject(FileLikeObject):
    """ A FileBackedObject is a stream which reads and writes physical
    files on disk.

    The file:// URL scheme is used.
    """
    def __init__(self, uri, mode):
        if not uri.startswith("file://"):
            Raise("You must have a fully qualified urn here, not %s" % urn)

        filename = uri[len("file://"):]
        escaped = escape_filename(filename)
        if mode == 'r':
            self.fd = open(escaped, 'rb')
        else:
            try:
                os.makedirs(os.path.dirname(escaped))
            except Exception,e:
                pass

            try:
                self.fd = open(escaped, 'r+b')
            except:
                self.fd = open(escaped, 'w+b')
                
        self.urn = uri
        self.mode = mode

    def seek(self, offset, whence=0):
        self.fd.seek(offset, whence)

    def tell(self):
        return self.fd.tell()

    def read(self, length=None):
        if length is None:
            return self.fd.read()
        
        result = self.fd.read(length)
        return result

    def write(self, data):
        self.fd.write(data)
        return len(data)

    def close(self):
        self.fd.close()

    def flush(self):
        self.fd.flush()


try:
    import pycurl

## TODO implement caching here
    class HTTPObject(FileLikeObject):
        """ This class handles http URLs.

        We support both download and upload (read and write) through
        libcurl. Uploading is done via webdav so you web server must
        be configured to support webdav.

        This implementation uses webdav to write the image on the server as
        needed. You can use a Zip volume or a directory volume as needed. The
        following is an example of how to set up apache to support
        webdav. Basically add this to the default host configuration file:

        <Directory "/var/www/webdav/" >
             DAV On
             AuthType Basic
             AuthName "test"
             AuthUserFile /etc/apache2/passwd.dav

             <Limit PUT POST DELETE PROPPATCH MKCOL COPY BCOPY MOVE LOCK \
             UNLOCK>
                Allow from 127.0.0.0/255.0.0.0
                Require user mic
                Satisfy Any
             </Limit>
        </Directory>

        This allows all access from 127.0.0.1 but requires an authenticated
        user to modify things from anywhere else. Read only access is allowed
        from anywhere.
        """
        handle = None
        
        def __init__(self, urn=None, mode='r'):
            FileLikeObject.__init__(self, urn, mode)
            if urn:
                if mode=='r':
                    self.handle = pycurl.Curl()
                    self.buffer = StringIO.StringIO()

                    def parse_header_callback(header):
                        ## we are looking for a Content-Range header
                        if header.lower().startswith("content-range: bytes"):
                            header_re = re.compile(r"(\d+)-(\d+)/(\d+)")
                            m = header_re.search(header)
                            if m:
                                self.size = long(m.group(3))

                    self.handle.setopt(pycurl.VERBOSE, 0)
                    self.handle.setopt(pycurl.URL, urn)
                    self.handle.setopt(pycurl.FAILONERROR, 1)
                    self.handle.setopt(pycurl.WRITEFUNCTION, self.buffer.write)
                    self.handle.setopt(pycurl.HEADERFUNCTION, parse_header_callback)

                    ## Make a single fetch from the url to work out our size:
                    self.read(1)
                    
                elif mode=='w':
                    self.handle = pycurl.Curl()
                    self.multi_handle = pycurl.CurlMulti()
                    self.send_buffer = ''

                    def read_callback(length):
                        result = self.send_buffer[:length]
                        self.send_buffer = self.send_buffer[length:]

                        return result

                    self.handle.setopt(pycurl.VERBOSE, 0)
                    self.handle.setopt(pycurl.URL, self.urn)
                    self.handle.setopt(pycurl.FAILONERROR, 1)
                    self.handle.setopt(pycurl.WRITEFUNCTION, lambda x: len(x))
                    self.handle.setopt(pycurl.INFILESIZE_LARGE, 0xFFFFFFFFL)
                    self.handle.setopt(pycurl.UPLOAD, 1)
                    self.handle.setopt(pycurl.READFUNCTION, read_callback)

                    self.multi_handle.add_handle(self.handle)
                
        def read(self, length=None):
            self.buffer.truncate(0)
            if length is None:
                length = self.size - self.readptr
                
            self.handle.setopt(pycurl.RANGE, "%d-%d" % (self.readptr, self.readptr + length))
            try:
                self.handle.perform()
            except pycurl.error,e:
                raise IOError("pycurl.error: %s" % e)

            result = self.buffer.getvalue()[:length]
            self.readptr += len(result)

            return result

        def flush(self):
            pass

        def write(self, data):
            if len(data)==0: return
            
            if self.mode!='w':
                Raise("Trying to write on an object opened for reading")
            
            self.send_buffer += data
            while 1:
                res, handle_count = self.multi_handle.perform()
                
                if handle_count==0:
                    time.sleep(1)

                if not self.send_buffer:
                    break

            self.readptr += len(data)
            self.size = max(self.size, self.readptr)
                
except ImportError:
    class HTTPObject(FileLikeObject):
        def __init__(self, urn=None, mode='r'):
            raise RuntimeError("HTTP streams are not implemented. You need to install libcurl python bindings (python-pycurl)")


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

    def __contains__(self, obj):
        return obj in self.hash

    def __getitem__(self, urn):
        return self.hash[urn]
    
class URNObject:
    """ A URNObject is an object in AFF4 space with a set of properties """
    def __init__(self, urn):
        self.properties = {}
        self.urn = urn
        self.lock = threading.Lock()

    def __str__(self):
        result = ''
        verbosity = int(oracle.resolve(GLOBAL, CONFIG_VERBOSE))
        for attribute,v in self.properties.items():
            for value in v:
                if verbosity <= _INFO and attribute.startswith(VOLATILE_NS): continue
                
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

    def export(self, prefix=''):
        result =''
        for attribute, v in self.properties.items():
            if not attribute.startswith(VOLATILE_NS):
                for value in v:
                    result += prefix + "%s=%s\n" % (attribute, value)
                
        return result

class Resolver:
    """ The resolver is a central point for storing facts about the universe """
    def __init__(self):
        self.urn = {}
        self.read_cache = Store()
        self.write_cache = Store()
        self.clear_hooks()

    def __getitem__(self, urn):
        try:
            obj = self.urn[urn]
        except KeyError:
            obj = URNObject(urn)

        self.urn[urn] = obj
        return obj

    def set(self, uri, attribute, value):
        DEBUG(_DEBUG, "Setting %s: %s=%s", uri, attribute, value);
        self[uri].set(attribute, value)
        
        ## Notify all interested parties
        for cb in self.set_hooks:
            cb(uri, attribute, value)
        
    def add(self, uri, attribute, value):
        DEBUG(_DEBUG, "Adding %s: %s=%s", uri, attribute, value);
        self[uri].add(attribute, value)

        ## Notify all interested parties
        for cb in self.add_hooks:
            cb(uri, attribute, value)

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
        try:
            if obj.mode == 'w':
                if obj.urn not in self.write_cache:
                    self.write_cache.add(obj.urn, obj)
            else:
                if obj.urn not in self.read_cache:
                    self.read_cache.add(obj.urn, obj)
        finally:
            ## Release the lock now
            try:
                self[obj.urn].lock.release()
                DEBUG(_DEBUG,"Released %s", obj.urn)
            except: pass

    def open(self, uri, mode='r', interface=None):
        DEBUG(_DEBUG, "oracle: Openning %s (%s)" % (uri,mode))
        """ Opens the uri returning the requested object.

        If interface is specified we check that the object we return
        implements the correct interface or return an error.
        """
        result = None

        ## If the uri is not complete here we guess its a file://
        if ":" not in uri:
            uri = "file://%s" % uri

        ## Check for links
        if oracle.resolve(uri, AFF4_TYPE) == AFF4_LINK:
            uri = oracle.resolve(uri, AFF4_TARGET)
        
        try:
            if mode =='r':
                result = self.read_cache[uri]
            else:
                result = self.write_cache[uri]
        except KeyError:
            pass

        if not result:
            ## Do we know what type it is?
            type = self.resolve(uri, AFF4_TYPE)
            if type:
                for scheme, prefix, handler in DISPATCH:
                    if prefix == type:
                        result = handler(uri, mode)
                        result.mode = mode
                        break

        if not result:
            ## find the handler according to the scheme:
            for scheme, prefix, handler in DISPATCH:
                if scheme and uri.startswith(prefix):
                    result = handler(uri, mode)
                    result.mode = mode
                    break

        if result:
            ## Obtain a lock on the object
            try:
                obj = self[uri]
            except KeyError:
                obj = self[uri] = URNObject(uri)

            DEBUG(_DEBUG, "Acquiring %s ",uri)
            obj.lock.acquire()
            DEBUG(_DEBUG, "Acquired %s", uri)

            ## Try to seek it to the start
            try:
                result.seek(0)
            except: pass
            
            return result

        ## We dont know how to open it.
        return NoneObject("Dont know how to handle URN %s. (type %s)" % (uri, type))

    def __str__(self):
        result = ''
        verbosity = int(oracle.resolve(GLOBAL, CONFIG_VERBOSE))
        keys = self.urn.keys()
        keys.sort()
        
        for urn in keys:
            obj = self.urn[urn]
            if verbosity <= _INFO and urn.startswith(VOLATILE_NS): continue
            attr_str = obj.__str__()
            if not attr_str: continue
            result += "\n****** %s \n%s" % (urn, attr_str)

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

            DEBUG(_WARNING,"Unknown line in properties: %s" % line)

    def export(self, subject):
        return self[subject].export()

    def export_all(self):
        result = ''
        for urn, obj in self.urn.items():
            result += obj.export(prefix=urn + " ")
        return result

    def register_add_hook(self, cb):
        """ Callbacks may be added here to be notified of add
        events. The callback will be called with the following
        prototype:

        cb(urn, attribute, value)
        """
        self.add_hooks.append(cb)

    def clear_hooks(self):
        self.add_hooks = []
        self.set_hooks = []
        
    def register_set_hook(self, cb):
        self.set_hooks.append(cb)
        
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
            try:
                fd.seek(self.base_offset + self.readptr)
                result = fd.read(length)
            finally:
                oracle.cache_return(fd)
                
            self.readptr += len(result)

            return result
        elif self.compression == ZIP_DEFLATED:
            Raise("ZIP_DEFLATED not implemented")

class ImageWorker(threading.Thread):
    """ This is a worker responsible for creating a full bevy """
    def __init__(self, urn, volume, bevy_number,
                 chunk_size, chunks_in_segment, condition_variable=None):
        """ Set up a new worker.

        The condition variable will be notified when we finish.
        """
        threading.Thread.__init__(self)

        self.condition_variable = condition_variable
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
        DEBUG(_DEBUG, "Starting thread")
        self.buffer.seek(0)
        offset = 0
        while self.buffer.tell() < self.buffer.len:
            data = self.buffer.read(self.chunk_size)
            cdata = zlib.compress(data, 1)
            self.bevy.write(cdata)
            self.bevy_index.write(struct.pack("<L", offset))
            offset += len(cdata)

        self.bevy_index.write(struct.pack("<L", 0xFFFFFFFF))

        subject = fully_qualified_name("%08d" % self.bevy_number, self.urn)

        ## we calculate the SHA (before we grab the lock on the
        ## volume)
        oracle.set(subject, AFF4_SHA,
                   hashlib.sha1(self.bevy.getvalue()).\
                   digest().encode("base64").strip())

        ## Grab the volume
        volume = oracle.open(self.volume, 'w')
        try:
           ## Write the bevy
            DEBUG(_INFO, "Writing bevy %s %s/%sMb (%d%%)" ,
                  subject,
                  (self.bevy.len / 1024 / 1024),
                  (self.bevy_size/ 1024 / 1024),
                  (100 * self.bevy.len) / self.buffer.len)
            volume.writestr(subject,
                            self.bevy.getvalue())
            volume.writestr(subject + '.idx',
                            self.bevy_index.getvalue())
        finally:
            ## Done
            oracle.cache_return(volume)

        ## Notify that we are finished
        self.condition_variable.acquire()
        self.condition_variable.notify()
        self.condition_variable.release()

        DEBUG(_DEBUG,"Finishing thread")
        
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
        self.condition_variable = threading.Condition()

        if mode=='w':
            self.writer = ImageWorker(self.urn, self.container, self.bevy_number,
                                      self.chunk_size, self.chunks_in_segment,
                                      condition_variable = self.condition_variable)

    def finish(self):
        oracle.set(self.urn, AFF4_TYPE, AFF4_IMAGE)
        oracle.set(self.urn, AFF4_INTERFACE, AFF4_STREAM)
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
            DEBUG(_DEBUG, "%s" % self.running)
            
            ## Update our view of the currently running threads
            while 1:
                number_of_threads = parse_int(oracle.resolve(GLOBAL, CONFIG_THREADS)) or 2
                self.running = [ x for x in self.running if x.is_alive() ]
                if len(self.running) >= number_of_threads:
                    self.condition_variable.acquire()
                    self.condition_variable.wait(10)
                    self.condition_variable.release()
                else: break
            
            self.bevy_number += 1
            self.writer = ImageWorker(self.urn, self.container, self.bevy_number,
                                      self.chunk_size, self.chunks_in_segment,
                                      condition_variable=self.condition_variable)

    def close(self):
        self.running.append(self.writer)
        self.writer.start()

        ## Wait untill all workers are finished
        for x in self.running:
            x.join()

        ## Write the properties file
        volume = oracle.open(self.container,'w')
        try:
            volume.writestr(fully_qualified_name("properties", self.urn),
                            oracle.export(self.urn))
        finally:
            oracle.cache_return(volume)

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
            try:
                data = fd.get_data()
                offset, length = struct.unpack(
                    "<LL", data[4 * chunk_in_bevy:4 * chunk_in_bevy + 8])
            finally:
                oracle.cache_return(fd)

            fd = oracle.open(bevy_urn, 'r')
            try:
                fd.seek(offset)
                chunk = zlib.decompress(fd.read(length))
            finally:
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
            stored = oracle.resolve(urn, AFF4_STORED) or \
                     Raise("Can not find storage for Volumes %s" % urn)
            
            try:
                self.load_from(stored)
            except IOError: pass
        else:
            AFFObject.__init__(self, urn, mode)
            
    def finish(self):
        oracle.set(self.urn, AFF4_TYPE, AFF4_ZIP_VOLUME)
        oracle.set(self.urn, AFF4_INTERFACE, AFF4_VOLUME)
        self.__init__(self.urn, self.mode)

    def writestr(self, subject, data, compress_type = ZIP_STORED):
        """ Write the filename on the archive.

        subject is a fully qualified name or relative to the current volume.
        """
        filename = escape_filename(relative_name(subject, self.urn))

        ## Where are we stored?
        stored = oracle.resolve(self.urn, AFF4_STORED)

        ## This locks the backing_fd for exclusive access
        backing_fd = oracle.open(stored,'w')
        try:
            ## Mark the file as dirty
            oracle.set(self.urn, AFF4_VOLATILE_DIRTY, '1')

            ## Where should we write the new file?
            directory_offset = parse_int(oracle.resolve(self.urn, AFF4_DIRECTORY_OFFSET)) or 0

            zinfo = zipfile.ZipInfo(filename = filename,
                                    date_time=time.gmtime(time.time())[:6])

            zinfo.external_attr = 0600 << 16L      # Unix attributes        
            zinfo.header_offset = directory_offset
            zinfo.file_size = len(data)
            zinfo.CRC = 0xFFFFFFFF & zlib.crc32(data)

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
            
        finally:
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
                                  int(time.time())
                
                count = count + 1
                dt = time.gmtime(zinfo.date_time)[:6]
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
                    filename, flag_bits = zinfo.filename, zinfo.flag_bits
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
        try:
            zf = zipfile.ZipFile(fileobj, mode='r', allowZip64=True)
        finally:
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
                DEBUG(_WARNING, "Volume does not have a valid URN - using temporary URN %s" % self.urn)
                
        oracle.add(self.urn, AFF4_STORED, filename)
        oracle.add(filename, AFF4_CONTAINS, self.urn)
        oracle.set(self.urn, AFF4_DIRECTORY_OFFSET, zf.start_dir)
        
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
        oracle.add(subject, AFF4_INTERFACE, AFF4_STREAM)

        oracle.add(subject, AFF4_VOLATILE_HEADER_OFFSET, zinfo.header_offset)
        oracle.add(subject, AFF4_VOLATILE_CRC, zinfo.CRC)
        oracle.add(subject, AFF4_VOLATILE_COMPRESSED_SIZE, zinfo.compress_size)
        oracle.add(subject, AFF4_SIZE, zinfo.file_size)
        oracle.add(subject, AFF4_VOLATILE_COMPRESSION, zinfo.compress_type)
        oracle.set(subject, AFF4_TIMESTAMP, int(time.mktime(zinfo.date_time +\
                                            (0,) * (9-len(zinfo.date_time)))))

        ## We just store the actual offset where the file is
        ## so we dont need to keep calculating it all the time
        oracle.add(subject, AFF4_VOLATILE_FILE_OFFSET, zinfo.header_offset +
                   sizeFileHeader + len(zinfo.filename))

class DirectoryVolume(AFFVolume):
    """ A directory volume is simply a way of storing all segments
    within a directory on disk.
    """

class Link(AFFObject):
    """ An AFF4 object which links to another object.

    When we open the link name, we return the AFF4_TARGET attribute.
    """            
    def finish(self):
        oracle.set(self.urn, AFF4_TYPE, AFF4_LINK)
        self.target = oracle.resolve(self.urn, AFF4_TARGET) or \
                      Raise("Link objects must have a target attribute")
        self.stored = oracle.resolve(self.urn, AFF4_STORED) or \
                      Raise("Link objects must be stored on a volume")

        return AFFObject.finish(self)

    def close(self):
        ## Make sure we write our properties file
        volume = oracle.open(self.stored, 'w')
        volume.writestr(fully_qualified_name("properties", self.urn),
                        oracle.export(self.urn))
        oracle.cache_return(volume)

class Point:
    def __init__(self, image_offset, target_offset, target_urn):
        self.image_offset = image_offset
        self.target_offset= target_offset
        self.target_urn = target_urn

    def __repr__(self):
        return "<%s,%s,%s>" % (self.image_offset, self.target_offset, self.target_urn)

class Map(FileLikeObject):
    """ A Map is an object which presents a transformed view of another
    stream.
    """
    def finish(self):
        oracle.set(self.urn, AFF4_TYPE, AFF4_MAP)
        oracle.set(self.urn, AFF4_INTERFACE, AFF4_STREAM)
        self.__init__(self.urn, self.mode)

    def __init__(self, uri=None, mode='r'):
        self.points = []
        if uri:
            self.stored = oracle.resolve(uri, AFF4_STORED) or \
                          Raise("Map objects must be stored somewhere")
            self.target = oracle.resolve(uri, AFF4_TARGET) or \
                          Raise("Map objects must have a %s attribute" % AFF4_TARGET)

            self.blocksize = parse_int(oracle.resolve(uri, AFF4_BLOCKSIZE)) or 1
            self.size = parse_int(oracle.resolve(uri, AFF4_SIZE)) or 1

            ## If a period is not specified we use the size of the
            ## image for the period. This allows us to use the same
            ## maths for both cases.
            max_size = max(self.size, parse_int(oracle.resolve(self.target, AFF4_SIZE)))
            self.image_period = self.blocksize * parse_int(
                oracle.resolve(uri, AFF4_IMAGE_PERIOD)) or max_size
            
            self.target_period = self.blocksize * parse_int(
                oracle.resolve(uri, AFF4_TARGET_PERIOD)) or max_size

        ## Parse the map now:
        if uri and mode=='r':
            fd = oracle.open("%s/map" % uri)
            fd.seek(0)
            line_re = re.compile("(\d+),(\d+),(.+)")
            try:
                for line in fd.read(fd.size).splitlines():
                    m = line_re.match(line)
                    if not m:
                        DEBUG(_WARNING, "Unable to parse map line '%s'" % line)
                    else:
                        self.points.append(Point(parse_int(m.group(1)),
                                                 parse_int(m.group(2)),
                                                 m.group(3)))
            finally:
                oracle.cache_return(fd)

        FileLikeObject.__init__(self, uri, mode)

    def bisect_right(self, image_offset):
        lo = 0
        hi = len(self.points)
        
        while lo < hi:
            mid = (lo+hi)//2
            if image_offset < self.points[mid].image_offset: hi = mid
            else: lo = mid+1
        return lo
                    
    def partial_read(self, length):
        """ Read from the current offset as much as possible - may
        return less than whats needed."""
        ## This function actually does the mapping
        period_number, image_period_offset = divmod(self.readptr, self.image_period)
        available_to_read = min(self.size - self.readptr, length)
        l = self.bisect_right(image_period_offset)-1
        target_offset = self.points[l].target_offset + \
                        image_period_offset - self.points[l].image_offset + \
                        period_number * self.target_period

        if l < len(self.points)-1:
            available_to_read = self.points[l+1].image_offset - image_period_offset
        else:
            available_to_read = min(available_to_read,
                                    self.image_period - image_period_offset)
                
        ## Now do the read:
        target = oracle.open(self.points[l].target_urn, 'r')
        try:
            target.seek(target_offset)
            data = target.read(available_to_read)
        finally:
            oracle.cache_return(target)

        self.readptr += len(data)
        return data

    def read(self, length):
        length = min(length, self.size - self.readptr)
        result = ''
        while len(result) < length:
            data = self.partial_read(length - len(result))
            if not data: break
            result += data

        return result

    def add(self, image_offset, target_offset, target_urn):
        self.points.append(Point(image_offset, target_offset, target_urn))

    def close(self):
        fd = StringIO.StringIO()
        def cmp_fun(x,y):
            return int(x.target_offset - y.target_offset)
        
        ## Sort the map array
        self.points.sort(cmp_fun)

        for i in range(len(self.points)):
            point = self.points[i]

            ## Try to reduce redundant points
            if i!=0 and i!=len(self.points) and \
                   self.points[i].target_urn == self.points[i-1].target_urn:
                previous = self.points[i-1]
                prediction = point.image_offset - previous.image_offset +\
                             previous.target_offset

                if prediction == point.target_offset: continue

            fd.write("%d,%d,%s\n" % (point.image_offset,
                     point.target_offset, point.target_urn))

        volume = oracle.open(self.stored, 'w')
        try:
            volume.writestr(fully_qualified_name("map", self.urn),
                            fd.getvalue())
            
            volume.writestr(fully_qualified_name("properties", self.urn),
                            oracle.export(self.urn))
        finally:
            oracle.cache_return(volume)

class Encrypted(FileLikeObject):
    def __init__(self, urn=None, mode='r'):
        raise RuntimeError("Encrypted streams are not implemented")

try:
    import M2Crypto

    class Identity(AFFObject):
        """ An Identity is an object which represents an X509 certificate"""
        x509 = None
        pkey = None

        ## These are properties that will be signed
        signable = set([AFF4_SIZE, AFF4_SHA])

        def __init__(self, urn=None, mode='r'):
            self.resolver = Resolver()
            
            if urn and not self.x509:
                ## Load the cert from the identity object
                fd = oracle.open(urn + "/cert.pem", 'r')
                try:
                    self.x509 = M2Crypto.X509.load_cert_string(fd.read())
                finally:
                    oracle.cache_return(fd)
                    
            AFFObject.__init__(self, urn, mode)

        def load_certificate(self, certificate_urn):
            fd = oracle.open(certificate_urn, 'r')
            try:
                certificate = fd.read()
                self.x509 = M2Crypto.X509.load_cert_string(certificate)
            finally:
                oracle.cache_return(fd)

        def load_priv_key(self, key_urn):
            fd = oracle.open(key_urn, 'r')
            try:
                self.pkey = M2Crypto.EVP.load_key_string(fd.read())
            finally:
                oracle.cache_return(fd)

        def finish(self):
            if not self.x509: return

            ## We set our urn from the certificate fingerprint
            self.urn = "%s/%s" % (AFF4_IDENTITY_PREFIX, self.x509.get_fingerprint())

            oracle.set(self.urn, AFF4_TYPE, AFF4_IDENTITY)

            ## Register an add hook with the oracle
            def add_cb(uri, attribute, value):
                if attribute in self.signable:
                    self.resolver.add(uri, attribute, value)

            def set_cb(uri, attribute, value):
                if attribute in self.signable:
                    self.resolver.set(uri, attribute, value)

            if self.x509 and self.pkey:
                ## Check that the private and public keys go together:
                if not self.x509.verify(self.pkey):
                    Raise("Public and private keys provided do not go with each other")

                oracle.register_add_hook(add_cb)
                oracle.register_set_hook(set_cb)

            AFFObject.finish(self)

        def verify(self, verify_cb = None):
            """ Loads all statements and verifies the hashes are correct """
            if not self.x509: Raise("No public key for identity %s" % self.urn)

            public_key = self.x509.get_pubkey()
            
            def cb(urn, attribute, value):
                ## This callback will be called whenever the parser
                ## reads a new property.
                if attribute == AFF4_SHA:
                    fd = oracle.open(urn, 'r')
                    digest = hashlib.sha1()
                    try:
                        while 1:
                            data = fd.read(1024 * 1024)
                            if not data: break
                            digest.update(data)

                        calculated = digest.digest()
                        value = value.decode("base64")
                    finally:
                        oracle.cache_return(fd)

                    verify_cb(urn, attribute, value, calculated)
                        
            self.resolver.register_add_hook(cb)
            
            for statement_urn in oracle.resolve_list(self.urn, AFF4_STATEMENT):
                ## Read and verify the statement
                statement = oracle.open(statement_urn, 'r')
                try:
                    statement_data = statement.read()
                finally: oracle.cache_return(statement)
                signature = oracle.open(statement_urn + ".sig", 'r')
                try:
                    sig_data = signature.read()
                finally: oracle.cache_return(signature)
                
                ## Now check the statement signature is correct:
                public_key.verify_init()
                public_key.verify_update(statement_data)
                if not public_key.verify_final(sig_data):
                    DEBUG(_WARNING, "Statement %s signature does not match!!! Skipping" % statement_urn)
                else:
                    self.resolver.parse_properties(statement_data)

        def close(self):
            if not self.pkey: return
            
            volume_urn = oracle.resolve(self.urn, AFF4_STORED) 
            volume = oracle.open(volume_urn, 'w')
            try:
                statement = self.resolver.export_all()

                ## Sign the statement
                if self.pkey:
                    self.pkey.sign_init()
                    self.pkey.sign_update(statement)
                    signature = self.pkey.sign_final()

                statement_urn = fully_qualified_name(str(uuid.uuid4()),
                                                     self.urn)
                oracle.set(self.urn, AFF4_STATEMENT, statement_urn)

                volume.writestr(statement_urn, statement)
                volume.writestr(statement_urn + ".sig", signature)
                volume.writestr(fully_qualified_name("properties", self.urn),
                                oracle.export(self.urn))

                ## Make sure to store the certificate in the volume
                cert_urn = fully_qualified_name("cert.pem", self.urn)
                cert = oracle.open(cert_urn, 'r')
                if cert: oracle.cache_return(cert)
                else:
                    text = self.x509.as_text() + self.x509.as_pem()
                    volume.writestr(cert_urn, text)

                ## Ensure that the volume knows it has an identity
                oracle.set(volume_urn, AFF4_IDENTITY_STORED, self.urn)
                
            finally:
                oracle.cache_return(volume)
except ImportError:
    class Identity(AFFObject):
        def __init__(self, urn=None, mode='r'):
            Raise("Signing is not available without the M2Crypto module. Try 'apt-get install python-m2crypto'")

## This is a dispatch table for all handlers of a URL method. The
## format is scheme, URI prefix, handler class.
DISPATCH = [
    [ 1, "file://", FileBackedObject ],
    [ 1, "http://", HTTPObject ],
    [ 0, AFF4_SEGMENT, ZipFileStream ],
    [ 0, AFF4_LINK, Link ],
    [ 0, AFF4_IMAGE, Image ],
    [ 0, AFF4_MAP, Map ],
    [ 0, AFF4_ENCRYTED, Encrypted ],
    [ 0, AFF4_IDENTITY, Identity],
    [ 0, AFF4_ZIP_VOLUME, ZipVolume ],
    ]


### These are helper functions
def load_volume(filename):
    """ Loads the volume in filename into the resolver """
    volume = ZipVolume(None, 'r')
    volume.load_from(filename)
    oracle.cache_return(volume)

    return volume.urn

## Set up some defaults
oracle.add(GLOBAL, CONFIG_VERBOSE, _INFO)
oracle.add(GLOBAL, CONFIG_THREADS, "2")

def DEBUG(verb, fmt, *args):
    if verb <= int(oracle.resolve(GLOBAL, CONFIG_VERBOSE)):
        print fmt % args

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
        try:
            self.assertEqual(fd.read(len(test_string)), test_string)
        finally:
            oracle.cache_return(fd)

    def test_02_HTTP(self):
        """ Test http object """
        url = "http://127.0.0.1/index.html"
        fd = HTTPObject(url)
        print "%s" % fd.read(10), fd.size
        fd.close()

        fd = HTTPObject("http://127.0.0.1/webdav/foobar.txt", 'w')
        fd.write("hello world")
        fd.close()

    def xtest_02_ZipVolume(self):
        """ Test creating of zip files """
        filename = "file://tests/test.zip"
        
        out_fd = ZipVolume(None, "w")
        oracle.add(out_fd.urn, AFF4_STORED, filename)
        out_fd.finish()
        out_fd.writestr("default","Hello world")
        out_fd.close()

    def xtest_02_ImageWriting(self):
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
        image_urn = image_fd.urn
        oracle.add(image_fd.urn, AFF4_STORED, volume_urn)
        image_fd.finish()
        in_fd = oracle.open(in_filename)
        try:
            while 1:
                data = in_fd.read(10240)
                if not data: break

                image_fd.write(data)
        finally:
            oracle.cache_return(in_fd)
            
        image_fd.close()
        
        ## Make a link
        link = Link(fully_qualified_name("default", volume_urn))
        oracle.set(link.urn, AFF4_STORED, volume_urn)
        oracle.set(link.urn, AFF4_TARGET, image_urn)
        link.finish()
        
        link.close()
        
        volume_fd = oracle.open(volume_urn, 'w')
        volume_fd.close()

    def xtest_03_ZipVolume(self):
        """ Tests the ZipVolume implementation """
        print "Loading volume"
        z = ZipVolume(None, 'r')
        z.load_from(ResolverTests.filename)

        ## Test the stream implementation
        fd = oracle.open(fully_qualified_name("properties", z.urn))
        try:
            data = z.zf.read("properties")
            self.assertEqual(data,fd.read(1024))
        finally:
            oracle.cache_return(fd)
            
        stream = oracle.open(fully_qualified_name("default", z.urn))
        try:
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
        finally:
            oracle.cache_return(stream)

if __name__ == "__main__":
    unittest.main()
