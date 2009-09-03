""" This is a binding for the raptor RDF library """
import sys, pdb
import ctypes.util
from ctypes import *

possible_names = ['libraptor', 'raptor',]
for name in possible_names:
    resolved = ctypes.util.find_library(name)
    if resolved:
        break

#Place a symlink to the debugged version of libraptor for debugging
#resolved = "./libraptor.so"
try:
    if resolved == None:
        raise OSError()
    libraptor = CDLL(resolved)
    if not libraptor._name: raise OSError()
except OSError:
    raise ImportError("libraptor not found. You might need to install it from ")

libc = CDLL(ctypes.util.find_library("c"))

## This is a cache of all serializer objects - this way we can access
## python objects from the C callbacks
SERIALIZERS_CACHE = {}

## Some declerations
libraptor.raptor_init.restype = None
libraptor.raptor_set_statement_handler.restype = None
libraptor.raptor_uri_filename_to_uri_string.restype = c_void_p
libraptor.raptor_new_uri.restype = c_void_p
libraptor.raptor_uri_copy.restype = c_void_p

class raptor_statement(Structure):
    _fields_ = [("subject", c_char_p),
                ("subject_type", c_int),
                ("predicate", c_char_p),
                ("predicate_type", c_int),
                ("object", c_char_p),
                ("object_type", c_int),
                ("object_literal_datatype", c_char_p),
                ("object_literal_language", c_int),
               ]              

class raptor_iostream_handler2(Structure):
    _fields_ = [ ("version", c_int),
                 ("init_func", c_void_p),
                 ("finish_func", c_void_p),
                 ("write_byte_func", c_void_p),
                 ("write_bytes_func", c_void_p),
                 ("write_end_func", c_void_p),
                 ]

## Create a default iostream handler
RAPTOR_IOSTREAM_HANDLER = raptor_iostream_handler2()
def write_bytes_func(self, data, size, len):
    serializer = SERIALIZERS_CACHE[self]
    serializer.write(string_at(data, size* len))
    
    return size * len

def write_byte_func(self, data):
    serializer = SERIALIZERS_CACHE[self]
    serializer.write(chr(data))
    return 1

RAPTOR_IOSTREAM_HANDLER.version = 2
RAPTOR_IOSTREAM_HANDLER.init_func = None
RAPTOR_IOSTREAM_HANDLER.finish_func = None
RAPTOR_IOSTREAM_HANDLER.write_end_func = None
RAPTOR_IOSTREAM_HANDLER.write_byte_func = cast(CFUNCTYPE(
    c_uint, c_void_p, c_int)(write_byte_func), c_void_p)
RAPTOR_IOSTREAM_HANDLER.write_bytes_func = cast(CFUNCTYPE(
    c_uint, c_void_p, c_char_p, c_int, c_int)(write_bytes_func), c_void_p)

## This is the prototype for the statement handler
STATEMENT_HANDLER = CFUNCTYPE(None, c_void_p, POINTER(raptor_statement))

## Initialise the library
libraptor.raptor_init()

class object_type:
    def __init__(self, value, datatype):
        self.value = value
        self.datatype = datatype
        
    def __str__(self):
        return self.value

    def serialize(self):
        return self.datatype, self.value

    def __int__(self):
        return int(self.datatype)

class RDFParser:
    def __init__(self, filename, format='trig'):
        self.parser = libraptor.raptor_new_parser(format.__str__())
        
        ## Need to keep a reference to the wrapped callback to prevent
        ## it from being gced.
        self.cb = STATEMENT_HANDLER(self.statement_callback)
        
        ## Register the CB with the parser
        libraptor.raptor_set_statement_handler(self.parser, None, self.cb)
        uri_string = libraptor.raptor_uri_filename_to_uri_string(c_char_p(filename))
        self.uri = libraptor.raptor_new_uri(uri_string)
        self.base_uri = libraptor.raptor_uri_copy(self.uri)

    def literal(self, object_string, obj_type):
        return object_type(object_string, obj_type)

    def sha256(self, object_string, object_type):
        try:
            return object_string.decode("hex")
        except: return object_string

    ## If the parsed triple contains a data type, we use this
    ## dispatcher to automatically create a python object
    datatype_dispatcher = {1: literal,
                           5: literal,
                           #"http://aff.org/2009/aff4#sha256": sha256
                           }
        
    def statement_callback(self, data, statement):
        s = statement[0]

        object_t = s.object_literal_datatype or s.object_type
        dispatch = self.datatype_dispatcher.get(object_t, self.literal)
        try:
            obj = dispatch(s.object, object_t)
        except TypeError:
            obj = dispatch(self, s.object, object_t)
            
        self.triple_cb(s.subject, s.predicate, obj)

    def triple_cb(self, subject, predicate, obj):
        """ This callback is called once per triple - you need to over
        ride it to do something with the triples"""
        print subject, predicate, obj

    def parse(self):
        libraptor.raptor_parse_file(self.parser, self.uri, self.base_uri)


class RDFSerialiser:
    def __init__(self, uri, format='turtle'):
        uri_string = libraptor.raptor_uri_filename_to_uri_string(c_char_p(uri))
        self.uri = libraptor.raptor_new_uri(uri_string)
        self.serializer = libraptor.raptor_new_serializer(format.__str__())
        if not self.serializer: raise RuntimeError("Unable to create serializer")

        ## We store a reference to ourselves so the hanlder can find
        ## us (we can only pass it integers)
        self.hash = abs(hash(self))
        SERIALIZERS_CACHE[self.hash] = self
        self.output_stream = libraptor.raptor_new_iostream_from_handler2(
            self.hash, byref(RAPTOR_IOSTREAM_HANDLER))

        libraptor.raptor_serialize_start_to_iostream(self.serializer,
                                                     self.uri,
                                                     self.output_stream)

    def __del__(self):
        try:
            ## Remove our cache reference
            del SERIALIZERS_CACHE[self.hash]
        except: pass

    def _serialize(self, statement):
        libraptor.raptor_serialize_statement(self.serializer, byref(statement))

    def serialize(self, subject, predicate, obj):
        """ Call this method with a python obj to have it serializable.

        If check if obj has a method:
           type, string = obj.serialize()

        if type is a string, we use it as a datatype, else if type is
        an int we use it as an object_type.
        """
        ## Make a new statement
        s = raptor_statement()
        s.subject = subject
        s.subject_type = 1
        s.predicate = predicate
        s.predicate_type = 1
        try:
            t, serialization = obj.serialize()
            s.object = serialization
            if type(t) == str:
                s.object_literal_datatype = t
                s.object_type = 5
            else:
                s.object_type = int(t)
        except AttributeError:
            s.object = str(obj)
            s.object_type = 1

        self._serialize(s)
        
    def write(self, data):
        """ This is the default write method - override this with
        something useful
        """
        sys.stdout.write(data)

    def end(self):
        libraptor.raptor_serialize_end(self.serializer)
        return ""

if __name__=='__main__':
    class Translator(RDFParser):
        def __init__(self, filename, format='trig'):
            self.output = RDFSerialiser(filename, format='ntriples')
            RDFParser.__init__(self, filename, format)

        def triple_cb(self, subject, predicate, obj):
            self.output.serialize(subject, predicate, obj)
            
    parser = Translator(sys.argv[1])
    parser.parse()
    print "%s" % parser.output.end()
