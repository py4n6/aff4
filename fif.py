import zipfile, struct, zlib, re
import time, binascii
import bisect, os, uuid, sys
import logging

## The version of this implementation of FIF
VERSION = "FIF1.0"
CONTENT_TYPE = 'application/x-fif-file'

## Alas python zlib grabs the GIL so threads dont give much. This
## should be fixed soon: http://bugs.python.org/issue4738
## For now we dont use threads
## NUMBER_OF_THREADS = 2

class Store:
    """ A limited cache implementation """
    def __init__(self, limit=5000000):
        """ limit is the maximum size of the cache.

        When exceeded we start to expire objects. We estimate the size
        of each object by calling a len() method on it - this is ok
        for strings but might not be good for large objects.
        """
        ## This is the maximum amount of memory we can use up:
        self.limit = limit
        self.cache = {}
        self.seq = []
        self.size = 0
        self.expired = 0

    def put(self, key, obj):
        self.cache[key] = obj
        self.size += len(obj)        
        self.seq.append(key)

        while self.size >= self.limit:
            key = self.seq.pop(0)
            self.size -= len(self.cache[key])
            self.expired += 1
            del self.cache[key]

    def get(self, key):
        return self.cache[key]

    def expire(self, key):
        try:
            del self.cache[key]
        except KeyError:
            pass

class properties:
    """ A class to manage the properties file.

    properties are key value pairs which go into a file named
    'properties':

    key=value

    One on each line. There can be multiple values of each key but
    key,value pairs are unique. If there are multiple values for the
    same key the order in which they appear is important and must be
    preserved, however the ordering of different keys is
    arbitrary. keys and values are seperated by '=' which may not
    appear in the key name. Keys are case insensitive, but values are
    case sensitive. Values may represent text in which case they are
    encoded in UTF8. If they represent binary data they should be
    encoded in base64.
    """
    def __init__(self, init_string=None, parent=None):
        self.properties = {}
        self.parent = parent
        if init_string:
            self.from_string(init_string)

    def __getitem__(self, key):
        result = self.properties[key]
        return result[0]

    def getarray(self, key):
        try:
            return self.properties[key]
        except KeyError:
            return []

    def __setitem__(self, key, value):
        try:
            if value not in self.getarray(key):
                self.properties[key].append(value)
                if self.parent != None:
                    self.parent._didModify = True
        except (KeyError, ValueError):
            self.properties[key] = [ value ]

    def from_string(self, string):
        for line in string.splitlines():
            k,v = line.split("=",1)
            self[k] = v

    def __str__(self):
        result = ''
        for k,v in self.items():
            result+="%s=%s\n" % (k,v)

        return result

    def items(self):
        """ FIXME:
        as the driver architecture makes a habit of taking
        arguments and turning them into properties, we filter out
        any property here that is malnamed. This wont work in the future
        when we move beyond aff2: as a prefix
        """
        for k,v in self.properties.items():
            if k.startswith("aff2"):
                for value in v:
                    yield (k,value)
                
    def update(self, p):
        for k,v in p.items():
            try:
                if self[k] != v:
                    self[k] = v
                    if self.parent != None:
                        self.parent._didModify = True
            except KeyError:
                self[k] = v

    def get(self, key, default=None):
        try:
            return self[key]
        except KeyError:
            return default

    def __delitem__(self, key):
        try:
            del self.properties[key]
        except KeyError:
            pass
        
    def set(self, key, value):
        del self[key]
        self[key] = value
        if self.parent != None:
            self.parent._didModify = True


class InfoStore:
    """ The InfoStore stores metadata. It is stored in quads, consisting of
        [source fiffile, stream identifier, property, value]
        It is useful to keep track of which FIFFile had which metadata in it -
        that way we can look for an image in a particular bag, or query
        a particular bag for a certain locally named image.
        """

    def __init__(self):
        self.quads = []
        self.bagindex = {}
        self.subjectindex = {}
        self.attributeindex = {}
        self.valueindex = {}

    def print_query(self, bag, s, p, o):
        pp = "Query: "
        for el in [bag, s, p, o]:
            if el == None:
                pp += "None, "
            else:
                pp += "%s, " % el

        pp += "\r\n"
        return pp
    
    def query(self, bag, s, p, o):
        logging.debug("Infostore: query= %s" % self.print_query(bag,s,p,o))
        result = []

        if bag != None:
            if bag not in self.bagindex.keys():
                bagindexes = []
            else:
                bagindexes = self.bagindex[bag]
        else:
            bagindexes = range(len(self.quads))

        if s != None:
            if s not in self.subjectindex.keys():
                sindexes = []
            else:
                sindexes = self.subjectindex[s]
        else:
            sindexes = range(len(self.quads))

        if p != None:
            if p not in self.attributeindex.keys():
                pindexes = []
            else:
                pindexes = self.attributeindex[p]
        else:
            pindexes = range(len(self.quads))

        if o != None:
            if o not in self.valueindex.keys():
                oindexes = []
            else:
                oindexes = self.valueindex[o]
        else:
            oindexes = range(len(self.quads))

        logging.debug("Infostore: Bagindexes: %s" % str(bagindexes))
        logging.debug("Infostore: Sindexes: %s" % str(sindexes))
        logging.debug("Infostore: pindexes: %s" % str(pindexes))
        logging.debug("Infostore: oindexes: %s" % str(oindexes))
        
        intersect = set(bagindexes) & set(sindexes) & set(pindexes) & set(oindexes)
        for el in intersect:
            result.append(self.quads[el])
        return self.normalise_quads(result)

    
    def query_values(self, o):
        if o != None:
            oindexes = self.valueindex[o]
        else:
            oindexes = range(len(self.quads))

        values = []
        for el in oindexes:
            value = self.quads[el][3]
            if value not in values:
                values.append(value)
        return values           

            
    def normalise_quads(self, quads):
        res = []
        for line in quads:
            found = False
            for line2 in res:
                if line2[0] == line[0] and line2[1] == line[1] and line2[2] == line[2] and line2[3] == line[3]:
                    found = True
            if not found:
                res.append(line)
        return res
    
    def add(self, bag, s, p, o):
        if type(o) == list:
            raise RuntimeError("Bad argument: ERROR adding (passed array) %s %s %s %s" % (bag, s, p, o))
        index = len(self.quads)
        self.quads.append([bag, s, p, o])
        if bag not in self.bagindex.keys():
            self.bagindex[bag] = []
        self.bagindex[bag].append(index)

        if s not in self.subjectindex.keys():
            self.subjectindex[s] = []
        self.subjectindex[s].append(index)

        if p not in self.attributeindex.keys():
            self.attributeindex[p] = []
        self.attributeindex[p].append(index)

        if o not in self.valueindex.keys():
            self.valueindex[o] = []
        self.valueindex[o].append(index)

    def add_properties(self, bag, subject, properties):
        for p,v in properties.items():
            if type(v) == list:
                for el in v:
                    self.add(bag, subject, p,el)
            else:
                self.add(bag, subject, p,v)
            
    def pretty_print(self):
        result = ''
        for bag in self.bagindex.keys():
            result += "Bag %s contains...\r\n" % bag
            for subject in self.subjectindex.keys():
                quads = self.query(bag, subject, None, None)
                count = 0
                for (b,s,p,o) in quads:
                    if count == 0:
                        result+="%s\t%s\t%s\n" % (s,p,o)
                    else:
                        result+="\t\t%s\t%s\n" % (p,o)

                    count = count + 1
            result += "\r\n"
        return result

class UnresolvedURNIterator:
    def __init__(self, resolver):
        self.resolver = resolver
        self.last_seen_value = None
        self.seen_values = []

    def __iter__(self):
        return self


    def is_URI(self, value):
        if value.startswith("urn:aff2") or value.startswith("file://"):
            return True
        else:
            return False
                                                            
    def next(self):
        values = self.resolver.infostore.query_values(None)
        for value in values:
            if value not in self.seen_values:
                self.seen_values.append(value)
                if self.is_URI(value):
                    subject_query = self.resolver.infostore.query(None,value, None,None)
                    if len(subject_query) == 0:
                        # found an URI that we dont know about
                        return value
        raise StopIteration
            
class Resolver():
    def __init__(self, rootContainer=None, autoload=False, infostore = None):
        if infostore == None:
            infostore = InfoStore()
        self.infostore = infostore
        self.volumes = {}
        if rootContainer != None and len(rootContainer.primary_names) != 0:
            self.volumes[rootContainer.primary_names[0]] = rootContainer
        if autoload:
            pass
        self.closing = False
        
        logging.debug("Resolver initialized.")

    def does_resolve(self, urn):
        files = self.infostore.query(None, urn, None, None)
        if len(files) == 0:
            return False
        else:
            return True

    def close(self, caller):
        logging.debug("Resolver: receiving close signal from %s" % str(caller))
        if not self.closing:
            closing = True
            for id in self.volumes.keys():
                try:
                    volume = self.volumes[id]
                    if volume == caller:
                        pass
                    else:
                        logging.debug("Resolver: closing %s" % str(volume))
                        del self.volumes[id]
                        volume.close()
                except KeyError:
                    # we may have deleted the item from the map from a recursive call - ignore
                    pass
        else:
            # this is a callback from another close
            pass
        
    def register(self, name, volume):
        # when creating FIFFiles we need to register them with the Resolver
        self.volumes[name] = volume

    def dump(self):
        logging.debug("Resolver: volumes map %s" % str(self.volumes))

    def resolve_file_reference(self, urn):
        filename = urn[len("file://"):]
        tmpfn = filename
        lastlength = len(filename)
        found = False
        while not found:
            if tmpfn.endswith(".zip") and os.path.exists(tmpfn):
                found = True
            else:
                tmpfn = os.path.split(tmpfn)[0]
                if len(tmpfn) == lastlength:
                    break
                lastlength = len(tmpfn)
                
        if found:
            if len(tmpfn) == len(filename):
                # we most likely dont have a path after the archive
                res = FIFFile([filename])
                self.volumes[res.primary_names[0]] = res
                return [res]
            else:
                archive = tmpfn
                local_name = filename[len(tmpfn)+1:]
                res = FIFFile([archive])
                self.volumes[res.primary_names[0]] = res
                stream = res.open_stream_by_name(local_name)
                return [stream]

        return []
    
    def resolve(self, urn):
        logging.debug("Resolver: Resolving %s" % urn)
        self.dump()
        results = []
        if not self.does_resolve(urn):
            if urn.startswith("file://"):
                try:
                    return self.resolve_file_reference(urn)
                except IOError:
                    pass
            else:
                logging.debug("Resolver: Resolution failed. Looking for storage hints")
                files = self.infostore.query(None, None, "aff2-storage:next_volume", None)
                
                subvolumes = []
                for quad in files:
                    fifURI = quad[3]
                    logging.debug("Resolver: storage hint: %s" % fifURI)
                    if fifURI not in self.volumes.keys():
                        res = self.load(fifURI)
                        if res != None:
                            self.volumes[res.primary_names[0]] = res
                            #return []
                            self.dump()
                            return self.resolve(urn)
        else:
            # find the
            quads = self.infostore.query(None, urn, "aff2-storage:type", None)
            for quad in quads:
                logging.debug("Resolver: URN %s is a %s, and located in %s" % (urn, quad[3], quad[0]))
                if quad[3] == "aff2-storage:Image":
                    image = self.volumes[quad[0]]
                    logging.debug("Resolver: image: %s" % str(image))
                    results.append(image.open_stream(urn))
        return results
    
    def load(self, URI):
        
        if URI.startswith("file://"):
            filename = URI[7:]
            logging.debug("Resolver: Loading %s" % filename)
            container = FIFFile([filename], autoload = False, readwrite = False, resolver=self, infostore=self.infostore)
            self.volumes[container.primary_names[0]] = container
            return container
        return None
    
    def tell(self, container, subject, attribute, value):
        self.store(container, subject, attribute, value)

    def unresolved_urns(self):
        return UnresolvedURNIterator(self)
        
                
class FIFFile(zipfile.ZipFile):
    """ A FIF file is just a Zip file which follows some rules.

    FIF files are made up of a series of volumes. Volumes are treated
    as parts of a single logical FIF file by consolidating all their
    Central Directory (CD) entries according to the following rules:

    - Multiple CD entries for the same file path are overridden by the
    latest entry according to the last modfied date and time fields in
    the CD.

    - CD Entries with compressed_size and uncompr_size of zero are
    considered to be deleted and removed from the combined CD.

    - Each volume MUST have a properties file at the top level. This
    properties file MUST have the same UUID attribute for volumes in
    the same logical FIF fileset. UUIDs are generated using RFC 4122.

    In this way we are able to extend and modify files within the FIF
    fileset by creating additional volumes. Use fifrepack to merge all
    volumes into a single volume and repack the volume to remove old
    files.
    """
    ## This is the file object which is currently being written to.
    fp = None

    ## This is an estimate of our current size
    size = 0

    ## This is a write lock for the current FIF file
    wlock = None

    _didModify = False
    start_dir = 0

    def __init__(self, filenames=[], parent=None, autoload = False, readwrite = True, resolver = None, infostore = None):
        """ Build a new FIFFile object.

        filenames specify a list of file names or file like object
        which will be opened as volumes. parent is another fif file
        which this file will be created as a child of (therefore we
        will share its UUID).

        autoload specifies if we should attempt to automatically load
        all volumes referenced by this volume.
        """
        ## This is an index of all files in the archive set
        if type(filenames)==str:
            filenames = [filenames]
        self.resolver = resolver
        self.readwrite = readwrite
        self.primary_names = []
        self.file_offsets = {}
        self.Store = Store()
        self.zipfiles = []
        self.properties = properties(parent=self)
        if infostore == None:
            self.info = InfoStore()
        else:
            self.info = infostore
            
        self.volume_directories = []
        self.properties['aff2-storage:version'] = VERSION
        self.properties["aff2-storage:type"] = "aff2-storage:AFF2Containter"
        if parent:
            self.properties.set('aff2-storage:UUID', parent.properties['aff2-storage:UUID'])
        
        ## This is used to keep track of the current write position in
        ## the file. This is because the file may be read and written
        ## at the same time. Read position is not important because
        ## its taken from the file_offsets index each time.
        self.readptr = 0
        
        ## This keeps track of the currently outstanding
        ## writers. Writers get notified with a flush() when the FIF
        ## file needs to do something, e.g. change volume. This gives
        ## them a chance to dump out critical information (e.g. key
        ## material). Note that writers are not expected to finalise
        ## their stream when we call flush() because we might switch
        ## to another volume and they can keep going, flushing
        ## typically only involves dumping enough information to be
        ## able to use the portion of the stream in this volume all by
        ## itself - in case other volumes get lost.
        self.writers = []
        
        ## These are the volumes that we currently process
        self.volumes = set()

        ## Initialise our baseclass for writing - writing is mainly
        ## handled by our base class, while reading is done directly
        ## using the file_offsets index.
        if self.readwrite == True:
            zipfile.ZipFile.__init__(self, None, mode='w',
                                     compression=zipfile.ZIP_DEFLATED,
                                     allowZip64=True)


        if resolver == None:
            self.resolver = Resolver(self, infostore=self.info)
            
        ## Load in any files given
        for f in filenames:
            name = self.get_canonical_file_name(f)
            self.merge_fif_volumes(name)
            self.primary_names.append(name)
            self.resolver.register(name, self)
            

        ## Now recursively load in any other volumes:
        #while autoload:
        #    volume = self.volume_loaded()
        #    ## No unresolved volumes left
        #    if not volume: break
        #
        #    self.merge_fif_volumes(volume)

        if autoload:
            for urn in self.resolver.unresolved_urns():
                for volume in self.resolver.resolve(urn):
                    self.merge_fif_volumes(volume)
                
        ## Now get a UUID if needed. We could have been provided a
        ## UUID from our parent or from any of the volume we loaded -
        ## but if not we need to come up with a new one:
        try:
            self.properties['aff2-storage:UUID']
        except KeyError:
            self.properties['aff2-storage:UUID'] = uuid.uuid4().__str__()
        if len(self.primary_names) != 0:
            subjectname = "urn:aff2:%s" % self.properties['aff2-storage:UUID']
            self.info.add_properties(self.primary_names[0], subjectname, self.properties)
        
        
    def get_canonical_file_name(self, name):
        return os.path.realpath(os.path.join(os.getcwd(), name))
    
    def volume_loaded(self):
        """ Returns another volume to be loaded from the current FIF
        set
        """
        try:
            ## First do the file:// volumes
            for volume in self.properties.getarray('volume'):
                if "file://" in volume and volume not in self.volumes:
                    return volume

            ## Now do everything else
            for volume in self.properties.getarray('volume'):
                if volume not in self.volumes:
                    return volume
                
            for volumedir in self.volume_directories:
                for file in os.listdir(volumedir):
                    url = "file://%s" % file
                    
                    if url not in self.volumes and self.is_fif_volume(os.path.join(volumedir, file)):
                        return os.path.join(volumedir, file)
                
        except KeyError:
            pass

    def is_fif_volume(self, file):
        if file.endswith(".zip"):
            zf = zipfile.ZipFile(file,
                             mode='r', allowZip64=True)
            infolist = zf.infolist()
            zf.close()
            for zinfo in infolist:
                if zinfo.filename == "properties":
                    #TODO: validate the fif version in the properties
                    return True
        return False
    
    def resolve_url(self, fileobj):
        """ Given a url, or stream name or file like object returns a
        file like object.
        """

        ## Its already file like object
        try:
            fileobj.seek
            fileobj.read
            fileobj.write
        except AttributeError:
            try:
                ## Could be a plain filename
                #print "opening %s", fileobj
                origname = fileobj
                fileobj = open(fileobj,'r+b')
                self.volume_directories.append(os.path.dirname(os.path.realpath(os.path.join(os.getcwd(), origname))))
            except IOError:
                ## it refers to an external file
                if fileobj.startswith("file://"):
                    return open(fileobj[len("file://"):],'r+b')
                else:
                    ## Maybe its a stream within this current FIF
                    ## fileset
                    try:
                        fileobj = self.open_stream(fileobj)
                    except KeyError:
                        raise IOError("Not found: %s" % fileobj)
                    
        return fileobj

    def merge_fif_volumes(self, url):
        """ Load and merge the FIF volumes specified. """
        fileobj = self.resolve_url(url)
        
        if fileobj not in self.zipfiles:
            self.zipfiles.append(fileobj)

        ## We parse out the CD of each file and build an index
        zf = zipfile.ZipFile(fileobj, mode='r', allowZip64=True)

        parentUUID = None

        infolist = zf.infolist()
        for zinfo in infolist:
            self.update_index(fileobj, zinfo)
            if zinfo.filename == "properties":
                filename = zinfo.filename

                p = properties(zf.read(filename))
                subjectname = "urn:aff2:%s" % p['aff2-storage:UUID']
                self.info.add_properties(url, subjectname, p)
                
                parentUUID = p['aff2-storage:UUID']
                self.properties.update(p)
                infolist.remove(zinfo)

        if parentUUID == None:
            raise RuntimeError("No volume properties file in FIF volume")

        for zinfo in infolist:
            filename = zinfo.filename
            if filename.endswith('properties'):
                p = properties(zf.read(filename))

                subjectname = "urn:aff2:%s" % p['aff2-storage:UUID']
                self.info.add_properties(url, subjectname, p)
                    
                ## Ok we add it to our volume set
                if type(fileobj)==file:
                    name = "file://%s" % os.path.basename(fileobj.name)
                else:
                    name = fileobj.name

                if name not in self.volumes:
                    self.volumes.add(name)
                    self.properties['aff2-storage:references'] = name
                    
                #self.properties.update(p)

    def update_index(self, fileobj, zinfo):
        """ Updates the offset index to point within the zip file we
        need to read from. This is done so we dont need to reparse the
        zip headers all the time (probably wont be needed in the C
        implementation - just that python is abit slow)
        """
        try:
            ## Update the index with the more recent zinfo
            row = self.file_offsets[zinfo.filename]
            if row[4] > zinfo.date_time:
                return
        except KeyError:
            pass

        ## Work out where the compressed file should start
        offset = zinfo.header_offset + len(zinfo.FileHeader())

        self.file_offsets[zinfo.filename] = ( fileobj, offset,
                                              zinfo.compress_size, zinfo.compress_type,
                                              zinfo.date_time)            

    def open_member(self, stream_name, mode='r',
                    compression=zipfile.ZIP_STORED):
        """ This opens an archive member for random access.

        We can either open the member in read mode or write mode. In
        write mode we receive a stream directly into the archive. You
        must call finalise() explicitely on the returned stream - this
        will cause the fileheaders to be finalised. Note that you can
        not open a compressed member for random access, but you can
        write a compressed member using the stream interface (as an
        alternative to writestr() - this is suitable for very large
        members).
        """
        zinfo = zipfile.ZipInfo(stream_name, date_time=time.gmtime())

        if mode == 'w':
            ## check the lock:
            if self.wlock:
                raise IOError("Zipfile is currently locked for writing member %s" % self.wlock)

            self.wlock = stream_name

            ## We are about to write an unspecified length member
            zinfo.compress_size = zinfo.file_size = zinfo.CRC = 0
            zinfo.flag_bits = 0x08
            zinfo.compress_type = compression
            zinfo.header_offset = self.readptr
            
            ## Write the zinfo header like that
            self.write(zinfo.FileHeader())

            return ZipFileStream(self, 'w', self.fp, zinfo, self.fp.tell())
        else:
            ## Get the offset ranges from the cache
            fp, offset, size, type, date_time = self.file_offsets[stream_name]
            if type == zipfile.ZIP_DEFLATED:
                raise RuntimeError("Unable to open compressed archive members for random access")
            
            zinfo.file_size = size

            return ZipFileStream(self, 'r', fp, zinfo, offset)
    
    def writestr(self, member_name, data, compression=zipfile.ZIP_STORED):
        """ This method write a component to the archive. """
        if not self.fp:
            raise RuntimeError("Trying to write to archive but no archive was set - did you need to call create_new_volume() or append_volume() first?")

        ## FIXME - implement digital signatures here
        data = data.__str__()
        m = self.open_member(member_name, 'w', compression = compression)
        m.write(data)
        m.close()

    def write(self, data):
        self.fp.seek(self.readptr)
        self.fp.write(data)
        self.readptr += len(data)
        self.size = max(self.size, self.readptr)
        self._didModify = True

    def read_member(self, filename):
        """ Read a file from the archive. We do memory caching for
        speed, and read the file directly from the fileobj.

        Note that we read the whole file here at once. This is only
        suitable for small files as large files will cause us to run
        out of memory. For larger files you should just call
        open_member() and then read().
        """
        try: 
            return self.Store.get(filename)
        except KeyError:
            ## Lookup the index for speed:
            zf, offset, length, compress_type, date_time = \
                     self.file_offsets[filename]

            zf.seek(offset)
            bytes = zf.read(length)

            ## Decompress if needed
            if compress_type == zipfile.ZIP_DEFLATED:
                dc = zlib.decompressobj(-15)
                bytes = dc.decompress(bytes)
                # need to feed in unused pad byte so that zlib won't choke
                ex = dc.decompress('Z') + dc.flush()
                if ex:
                    bytes = bytes + ex
                    
            self.Store.put(filename, bytes)
            return bytes

    def append_volume(self, filename):
        """ Append to this volume """
        ## Close the current file if needed:
        self.finalise()
        filename = self.get_canonical_file_name(filename)
        ## We want to set this filename as the current file to write
        ## on. Therefore, we need to reload the CD from that file so
        ## we can re-write the modified CD upon a flush() or close().
        for z in self.zipfiles:
            if z.name == filename:
                ## There it is:
                self.fp = z
                ## Rescan the CD
                self._RealGetContents()
                ## We start to add members at this point:
                self.readptr = self.start_dir
                return
            
        raise RuntimeError("Cant append to file %s - its not part of the FIF set" % filename)

    def create_new_volume(self, filename):
        """ Creates a new volume called filename. """
        ## Close the current file if needed:
        self.finalise()
        
        ## Were we given a filename or a file like object?
        try:
            filename.seek
            filename.read
            filename.write
            filename = self.get_canonical_file_name(filename.name)
            self.primary_names.append(filename)
        except AttributeError:
            try:
                filename = self.get_canonical_file_name(filename)
                self.primary_names.append(filename)
                filename = open(filename,'r+b')
                raise RuntimeError("The file %s already exists... I dont want to over write it so you need to remove it first!!!" % filename)
            except IOError:
                filename = open(filename,'w+b')

        ## Remember it
        self.zipfiles.append(filename)
        
        self.size = 0
        self.readptr = 0
        
        ## We now will be operating on this - reinitialise ourselves
        ## for writing on it (this will clear out of CD list):
        zipfile.ZipFile.__init__(self, filename, mode='w', compression=zipfile.ZIP_DEFLATED,
                                 allowZip64 = True)

    def flush(self):
        ## Tell all our outstanding writers to flush:
        for x in self.writers:
            x.flush()

        #try:
        #    self.fp.flush()
        #except AttributeError:
        #    pass
        
    def finalise(self):
        """ Finalise any created volume """
        if self._didModify and self.fp:
            try:
                self.properties['aff2-storage:UUID']
            except KeyError:
                self.properties['aff2-storage:UUID'] = uuid.uuid4().__str__()

            ##self.properties.set("current_volume", self.current_volume)
                
            self.writestr("properties", self.properties)

            self.flush()
            
            ## Call our base class to close us - this will dump out
            ## the CD
            self.fp.seek(self.readptr)
            zipfile.ZipFile.close(self)

    def close(self):
        self.finalise()
      
        for f in self.zipfiles:
            f.close()
            self.zipfiles.remove(f)
        self.resolver.close(self)
        
    def create_stream_for_writing(self, id=None, stream_name=None,
                                  stream_type='aff2-storage:Image',
                                  **args):
        """ This method creates a new stream for writing in the
        current FIF archive.

        We essentially instantiate the driver named by stream_type for
        writing and return it.
        """
        try:
            props = args['properties']
            del args['properties']
        except:
            props = properties()

        if id == None:
            id = uuid.uuid4().__str__()
        elif id.startswith("urn:aff2:"):
            id = id.replace("urn:aff2:", "")

        if stream_name == None:
            stream_name = id

        props["aff2-storage:UUID"] = id

        # HACK. I dont know why the following statement fails...
        #stream.properties["aff2-storage:name"] = args['stream_name']
        passedprops = properties()
        passedprops.update(args)
        if passedprops.get('local_name') != None:
            props["aff2-storage:name"] = passedprops.get('local_name')
        
        stream = types[stream_type]

        new_stream = stream(fd=self, stream_name=stream_name, mode='w',
                            properties=props, **args)
        self.writers.append( new_stream)
        self.info.add_properties(self.primary_names[0], new_stream.getId(), props)
        return new_stream

    def open_stream_by_name(self, stream_name):
        results = []

        for name in self.primary_names:
            queryresult = self.info.query(name, None, "aff2-storage:name", stream_name)
            for result in queryresult:
                results.append(result)

        if len(results) > 1:
            raise IOError("Open by name failed for %s: local name used more than once" % stream_name)
        if len(results) == 0:
            raise IOError("Open by name failed for %s: name not found" % stream_name)

        logging.debug("FIFFile:open_stream_by_name: Opening stream name %s : %s" % (stream_name, results[0][1]))
        return self.open_stream(results[0][1])
        
    def open_stream(self, stream_name):
        """ Opens the specified stream from out FIF set.

        We basically instantiate the right driver and return it.
        """
        logging.debug("FIFFile:open_stream: pening stream name: %s" % stream_name)
        if stream_name.startswith("urn:aff2:"):
            stream_name = stream_name.replace("urn:aff2:", "")
        
        props = properties(self.read_member("%s/properties" % stream_name))

        stream = types[props['aff2-storage:type']]
        return stream(fd=self, stream_name=stream_name, mode='r',
                      properties=props, parent=self)

    def stats(self):
        logging.info("Store load %s chunks total %s (expired %s)" % (
            len(self.Store.seq),
            self.Store.size,
            self.Store.expired,
            ))

    def __del__(self):
        self.close()

    def get_images(self):
        results = []
        for name in self.primary_names:
            for result in self.info.query(name, None, "aff2-storage:containsImage", None):
                results.append(result[3])
        return results

class FIFFD:
    """ A baseclass to facilitate access to FIF Files"""
    def write_properties(self):
        """ This returns a formatted properties string. Properties are
        related to the stream. target is a zipfile.
        """
        ## This will change as we go along so we only want one of
        ## these:
        self.properties.set('aff2-storage:size', self.size)
        
        ## Make the properties file
        filename = "%s/properties" % self.stream_name
        
        self.fd.writestr(filename, self.properties)

    def seek(self, pos, whence=0):
        #print "Seeking %s, %s %s" % (pos, whence, self.size)
        if whence==0:
            self.readptr = pos
        elif whence==1:
            self.readptr += pos
        elif whence==2:
            self.readptr = self.size + pos

    def tell(self):
        return self.readptr

    def flush(self):
        """ This gets called when we need to ensure that volatile
        stuff is on disk. For example, when changing volumes.
        """
        ## By default we write multiple copies of the properties
        ## file. We do this so that each volume is completely stand
        ## alone.
        if self.mode == 'w':
            self.write_properties()
        
    def close(self):
        ## Remove ourselves from the parent's outstanding writers list
        self.flush()
        try:
            idx = self.fd.writers.index(self)
            self.fd.writers.pop(idx)
        except IndexError:
            pass
        except ValueError:
            pass

    def getId(self):
        return "urn:aff2:%s" % (self.properties["aff2-storage:UUID"])
    
class InfoDriver(FIFFD):
    """ A stream writer for recording general informaiton """
    type = "aff2-storage:Info"
    size = 0
    
    def __init__(self, mode='w', stream_name='data',
                 attributes=None, fd=None,
                 properties = None, **args):
        #properties.update(args)
        self.fd = fd
        self.mode = mode
        self.name = self.stream_name = stream_name
        self.properties = properties
        self.properties["aff2-storage:type"] = "aff2-storage:Info"
        
    def write_properties(self):
        """ This returns a formatted properties string. Properties are
        related to the stream. target is a zipfile.
        """
       
        ## Make the properties file
        filename = "%s/properties" % self.stream_name

        self.fd.writestr(filename, self.properties)
        
    def close(self):
        FIFFD.close(self)


        
class ZipFileStream(FIFFD):
    """ This is a file like object which represents a zip file member """
    def __init__(self, parent,mode, fp, zinfo, file_offset):
        ## This is the FIFFile we belong to
        self.fp = fp
        self.mode = mode
        self.readptr = 0
        self.size = zinfo.file_size
        self.zinfo = zinfo
        self.file_offset = file_offset
        self.parent = parent
        if zinfo.compress_type == zipfile.ZIP_DEFLATED:
            self.compr = zlib.compressobj(9,
                                          zlib.DEFLATED, -15)
        else:
            self.compr = None

    def read(self, length=None):
        if self.compr: raise RuntimeError("Can not read compressed member opened for writing")
        if length==None: length = sys.maxint
        
        length = min(length, self.size - self.readptr)

        self.fp.seek(self.readptr + self.file_offset)
        data = self.fp.read(length)
        self.readptr += len(data)
        return data

    def seek(self, off, whence=0):
        if self.compr:
            raise RuntimeError("Unable to seek in compressed streams")
        
        FIFFD.seek(self,off, whence)

    def write(self, data):
        if self.mode != 'w': raise RuntimeError("Stream not opened for writing")

        ## Update the zinfo:
        self.zinfo.CRC = binascii.crc32(data, self.zinfo.CRC)
        self.zinfo.file_size += len(data)

        if self.compr:
            data = self.compr.compress(data)

        self.zinfo.compress_size += len(data)    
        self.parent.write(data)
                
    def close(self):
        if self.mode == 'r': return

        ## Flush decompressor
        if self.compr:
            data = self.compr.flush()
            self.zinfo.compress_size += len(data)    
            self.parent.write(data)
            chunk_size = self.zinfo.compress_size
        else:
            chunk_size = self.zinfo.file_size
            
        ## Write the data descriptor
        self.parent.write(struct.pack("<lLL", self.zinfo.CRC,
                                      self.size,
                                      self.size,))

        ## Add the new zinfo to the CD
        self.parent.filelist.append(self.zinfo)
        self.parent.NameToInfo[self.zinfo.filename] = self.zinfo

        ## Invalidate cache
        self.parent.Store.expire(self.zinfo.filename)

        ## Add to the read cache
        self.parent.file_offsets[self.zinfo.filename] = (
            self.parent.fp,
            ## Offset to the start of the file (after the header)
            self.file_offset,
            chunk_size,
            self.zinfo.compress_type,
            self.zinfo.date_time )
        
        ## Remove the write lock from the parent
        self.parent.wlock = False

    def tell(self):
        return self.readptr

class Image(FIFFD):
    """ A stream writer for creating a new FIF archive """
    type = "aff2-storage:Image"
    size = 0
    
    def __init__(self, mode='w', stream_name='data',
                 attributes=None, fd=None,
                 properties = None, **args):
        
        self.fd = fd
        self.mode = mode
        self.name = self.stream_name = stream_name
        self.chunk_id = 0
        self.chunksize = int(properties.get('aff2-storage:chunk_size', 32*1024))
        self.size = int(properties.get('aff2-storage:size',0))
        self.readptr = 0
        self.Store = Store()
        self.stream_re = re.compile(r"%s/(\d+)+.dd" % self.stream_name)
        self.outstanding = ''

        properties["aff2-storage:type"] = "aff2-storage:Image"

        ## The following are mandatory properties:
        self.properties = properties
        

    def read(self, length=None):
        #print "length ",self.readptr, length, 
        if length==None: length=sys.maxint
        result = ''
        length = min(self.size - self.readptr, length)
        while length>0:
            data= self.partial_read(length)
            if len(data)==0: break
            
            length -= len(data)
            result += data
            self.readptr += len(data)

        #print len(result), self.readptr, "%r" % result[-50:], self.size
        return result
    
    def partial_read(self, length):
        ## which chunk is it?
        chunk_id = self.readptr / self.chunksize
        chunk_offset = self.readptr % self.chunksize
        available_to_read = min(self.chunksize - chunk_offset, length)

        chunk = self.read_chunk(chunk_id)
        #print "chunk_id", chunk_id, len(chunk), chunk_offset, 
        return chunk[chunk_offset:chunk_offset+available_to_read]

    def read_chunk(self, chunk_id):
        return self.fd.read_member(self.make_chunk_name(chunk_id))

    def write_properties(self):
        self.properties.set('aff2-storage:count', self.chunk_id)
        FIFFD.write_properties(self)
        
    def make_chunk_name(self, chunk_id):
        return "%s/%08d.dd" % (self.stream_name, chunk_id)

    def write(self, data):
        self.readptr += len(data)
        self.size = max(self.size, self.readptr)
        #print "%s Writing %s, %s %r" % (self, self.readptr, len(data), data[-50:])
        data = self.outstanding + data
        while len(data)>self.chunksize:
            chunk = data[:self.chunksize]
            self.write_chunk(chunk)
            data = data[self.chunksize:]

        self.outstanding = data
        #print "Outstanding %s" % len(self.outstanding)
        
    def write_chunk(self, data):
        """ Adds the chunk to the archive """
        name = self.make_chunk_name(self.chunk_id)
        self.fd.writestr(name, data, zipfile.ZIP_DEFLATED)
        self.chunk_id += 1

    def close(self):
        """ Finalise the archive """
        #print "Closing archive with %s %s %s" % (len(self.outstanding),self.size, self.readptr)
        ## Write the last chunk
        if len(self.outstanding)>0:
            self.write_chunk(self.outstanding)
            
        FIFFD.close(self)

    def getId(self):
        return "urn:aff2:%s" % (self.properties["aff2-storage:UUID"])
        
class Overlay(Image):
    """ The overlay stream allows for external chunk references so we
    can just piggy back on top of files like EWF.
    """
    type = 'aff2-storage:Overlay'
    def __init__(self, mode='w', *args, **kwargs):
        self.chunks = {}
        self.filenames = {}
        self.inverted_filenames = {}
        self.overlay_count = 0
        Image.__init__(self, mode=mode, *args, **kwargs)
        if mode=='r':
            self.open_overlay()
        self.properties["aff2-storage:type"] = 'aff2-storage:Overlay'

    def open_overlay(self):
        self.targets = []
        for t in self.properties.getarray('aff2-storage:target'):
            self.targets.append(self.fd.resolve_url(t))
        
        for segment in self.properties.getarray('aff2-storage:overlay'):
            data = self.fd.read_member("%s/%s" % (self.stream_name, segment))
            for line in data.splitlines():
                id, offset, size, compression, filename_id = line.split(",",5)
                id = int(id)
                offset = long(offset)
                size = long(size)
                filename_id = int(filename_id)
                compression = int(compression)
                self.chunks[id] = (offset, size, compression, filename_id)

    def set_chunk(self, chunk_id, offset, size, compression, filename):
        try:
            filename_id = self.filenames[filename]
        except KeyError:
            ## Set a new filename
            filename_id = len(self.filenames.keys())
            self.filenames[filename] = filename_id
            self.inverted_filenames[filename_id] = filename

        self.chunks[chunk_id] = (offset, size, compression, filename_id)
        self.chunk_id = max(self.chunk_id, chunk_id)

    def close(self):
        ## We add the attributes
        for i in range(len(self.inverted_filenames.keys())):
            self.properties['aff2-storage:target'] = 'file://%s' % self.inverted_filenames[i].strip()

        name = "overlay.%02d" % (self.overlay_count)
        self.overlay_count += 1
        fd = self.fd.open_member("%s/%s" % (self.stream_name, name),
                                 mode="w",
                                 compression=zipfile.ZIP_DEFLATED)

        for i in range(self.chunk_id+1):
            try:
                row = self.chunks[i]
                fd.write("%s,%s\n" % (i,",".join(["%d" % x for x in row])))
            except Exception,e:
                print e
                pass

        fd.close()
        self.properties['aff2-storage:overlay'] = name
        Image.close(self)
        
    def read_chunk(self, chunk_id):
        (offset, size, compression, filename_id) = self.chunks[chunk_id]

        target = self.targets[filename_id]
        target.seek(offset)
        data = target.read(size)
        
        if compression:
            data = zlib.decompress(data)
            ## Decompress it now:
            #dc = zlib.decompressobj(-15)
            #data = dc.decompress(data)
            # need to feed in unused pad byte so that zlib won't choke
            #ex = dc.decompress('Z') + dc.flush()
            #if ex:
            #    data = data + ex
                
        return data
    
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

    - image_period - number of bytes in the target image each period
      will advance by. (Useful for RAID)
    
    """
    type = "aff2-storage:Map"
    
    def __init__(self, fd=None, mode='r', stream_name='data',
                 properties=None, parent=None, **args):

        properties.update(args)
        ## Build up the list of targets
        self.mode = mode
        self.targets = {}
        self.parent = parent
        
        try:
            count = 0

            targets = []
            if len(properties.getarray('target')) > 0:
                targets = properties.getarray('target')
            else:   
                targets = properties.getarray('aff2-storage:target')

            for target in targets:
                ## We can not open the target for reading at the same
                ## time as we are trying to write it - this is
                ## danegerous and may lead to file corruption.
                properties['aff2-storage:target'] = target
                if mode!='w':
                    logging.debug("Map:__init__: Resolving target %s" % target)
                    containers = self.parent.resolver.resolve(target)
                    logging.debug("Map:__init__: Resolved target %s to %s" % (target, str(containers)))
                    self.targets[count] = containers[0]
                    
                count +=1
                
        except KeyError:
            pass

        if count==0:
            raise RuntimeError("You must set some targets of the mapping stream")

        ## Get the underlying FIFFile
        self.fd = fd
        
        ## This holds all the file offsets on the map in sorted order
        self.points = []

        ## This holds the image offsets corresponding to each file offset.
        self.mapping = {}

        ## This holds the target index for each file offset
        self.target_index = {}

        properties["aff2-storage:type"] = "aff2-storage:Map"
        self.properties = properties
        self.stream_name = stream_name
        self.readptr = 0
        self.size = int(properties.get('aff2-storage:size',0))
        ## Check if there is a map to load:
        if mode=='r':
            self.load_map()
        
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
            file_period = int(self.properties['aff2-storage:file_period'])
            image_period = int(self.properties['aff2-storage:image_period'])
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

    def read(self, length=None):
        if length==None: length=sys.maxint
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
        result = ''
        for x in self.points:
            result += "%s %s %s\n" % (x, self.mapping[x], self.target_index[x])

        filename = "%s/map" % self.stream_name
        self.fd.writestr(filename, result, zipfile.ZIP_DEFLATED)

    def load_map(self):
        """ Opens the mapfile and loads the points from it """
        filename = "%s/map" % self.stream_name
        result =self.fd.read_member(filename)

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


class NULLScheme:
    """ This is the NULL encryption Scheme """
    def __init__(self, fiffile, stream_name, properties):
        self.master_key = None

    def encrypt_block(self, count, block):
        return block

    def decrypt_block(self, count, block):
        return block

crypto_schemes = {"null": NULLScheme,}

try:
    import Crypto.Hash.MD5
    import Crypto.Hash.SHA

    import Crypto.Cipher.AES
    import Crypto
    import Crypto.Util.randpool

    ## This is a crypto pool for key material
    POOL = Crypto.Util.randpool.RandomPool()

    class AES_SHA_PSK(NULLScheme):
        hash_class = Crypto.Hash.SHA.new
        crypt_class = Crypto.Cipher.AES.new
        mode = Crypto.Cipher.AES.MODE_CBC
        ## This is the cipher's block size
        block_size = 16
        key_size = 16
        pad = 0
        
        def __init__(self, fiffile, stream_name, properties):
            ## We derive the master key from PSK here
            self.properties = properties
            self.get_master_key()

        def get_master_key(self):
            try:
                salt = self.properties['aff2-storage:salt'].decode("base64")
            except KeyError:
                salt = POOL.get_bytes(8)
                self.properties['aff2-storage:salt'] = salt.encode("base64").strip()
                
            try:
                PSK = os.environ['FIF_PSK']
                print "Read PSK from environment"
            except KeyError:
                try:
                    PSK = self.properties['aff2-storage:PSK']
                    ## Clear the PSK from the properties so it doesnt
                    ## get written to file
                    del self.properties['aff2-storage:PSK']
                except KeyError:
                    PSK = raw_input("Type in a password:")

            self.master_key = self.hash_class(PSK+salt).digest()[:self.key_size]

        def encrypt_block(self, count, block):
            self.pad = len(block) % self.block_size
            if self.pad:
                #print "Padding from %s to %s" % (len(block),self.pad)
                block += "\xFF" * (self.block_size -self.pad)
                
            IV = self.hash_class(struct.pack("<L",count) + \
                                 self.master_key).digest()[:self.key_size]

            aes = self.crypt_class(self.master_key, self.mode,
                                   IV)

            return aes.encrypt(block)

        def decrypt_block(self, count, block):
            IV = self.hash_class(struct.pack("<L",count) + \
                                 self.master_key).digest()[:self.key_size]

            aes = self.crypt_class(self.master_key, self.mode,
                                   IV)

            return aes.decrypt(block)

    crypto_schemes['aes-sha-psk'] = AES_SHA_PSK

except ImportError:
    Crypto = None

class Encrypted(Image):
    type = "aff2-storage:Encrypted"
    fiffile = None

    def __init__(self, stream_name='crypt', properties = None, blocksize=4096,
                 **args):
        Image.__init__(self, stream_name=stream_name,
                       properties = properties, **args)
        print properties
        try:
            scheme = properties['aff2-storage:scheme']
        except KeyError:
            scheme = 'null'
            print "No scheme specified, defaulting to %s" % scheme
            properties['aff2-storage:scheme'] = scheme

        try:
            self.crypto = crypto_schemes[scheme]
        except KeyError:
            raise RuntimeError("Crypto scheme %s not implemented" % scheme)
        
        ## Default chunksize for encrypted is 16MB
        self.blocksize = int(blocksize)
        self.crypto = self.crypto(self.fd, self.stream_name, self.properties)
        self.Store = Store()
        self.properties["aff2-storage:type"] = "aff2-storage:Encrypted"
            
    def write_chunk(self, data):
        #print "Writing encrypted chunk %s %s" % (self.chunk_id, self.size)
        name = self.make_chunk_name(self.chunk_id)
        data = self.crypto.encrypt_block(self.chunk_id, data)
        self.fd.writestr(name, data, zipfile.ZIP_STORED)
        self.chunk_id += 1

    def read_chunk(self, chunk_id):
        name = self.make_chunk_name(chunk_id)
        try:
            data = self.Store.get(name)
        except KeyError:
            data = self.fd.read_member(name)
            data = self.crypto.decrypt_block(chunk_id, data)
            self.Store.put(name, data)
            
        return data

    def create_new_volume(self):
        """ This is a convenience method for creating a fif file
        inside the encrypted stream.
        """
        ## We contain a fif file
        self.properties['aff2-storage:content-type'] = CONTENT_TYPE
        self.fd.properties['aff2-storage:volume'] = self.stream_name
        
        self.fiffile = FIFFile(parent = self.fd, resolver=self.resolver)
        self.fiffile.create_new_volume(self)

        return self.fiffile

    def close(self):
        if self.properties.get('aff2-storage:content-type') == CONTENT_TYPE:
        ## Make sure that our container is closed
            self.fiffile.close()

        Image.close(self)

## The following are the supported segment types
types = {"aff2-storage:Image" : Image, "aff2-storage:Map" : MapDriver, "aff2-storage:Encrypted" : Encrypted,
             "aff2-storage:Overlay" : Overlay, "aff2-storage:Info" : InfoDriver}
