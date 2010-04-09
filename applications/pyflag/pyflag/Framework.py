""" Central file for all the miscelaneous functionality which doesnt
fit anywhere else.
"""
import Registry
import pyaff4
import pdb
import Store,re
from PyFlagConstants import *

import conf
config=conf.ConfObject()

STORE = Store.Store()

class EventHandler:
    """ An event handler object allows plugins to register their
    interst in being notified about sepecific events in the
    application's life.
    """
    def startup(self):
        """ This method is called when we first start """

    def finish(self):
        """ This method is called when we are finished processing """

    def create_volume(self):
        """ Run tasks on a newly created AFF4 volume """

    def exit(self):
        """ This event happens before we exit """

def post_event(event, *args, **kwargs):
    """ A Utility function to post the event to all listeners. """
    for e in Registry.EVENT_HANDLERS.classes:
        e = e()
        method = getattr(e, event)
        method(*args, **kwargs)

oracle = pyaff4.Resolver()

## The following are extensions of basic AFF4 types for doing pyflag
## specific things
class Proxy:
    """ A generic proxy class for proxying some other object """
    proxied = None

    def __getattr__(self, attr):
        """ This is only useful for proper methods (not ones that
        start with __ )
        """
        # Don't do a __nonzero__ check on proxied or things like '' will fail
        if self.proxied is None:
            raise AttributeError("No proxied object set")

        return getattr(self.proxied, attr)

    def get_metadata(self, attribute):
        return oracle.resolve_alloc(self.urn, attribute)

    def set_metadata(self, attribute, value, rdftype=None):
        RESULT_VOLUME.set_metadata(self.urn, attribute, value, rdftype)

class PyFlagMap(Proxy):
    pyaff4_class = pyaff4.MapDriver

    def __init__(self, path, base = '', navigatable = True, *args, **kwargs):
        self.proxied = self.pyaff4_class(*args, **kwargs)

        self.urn.set(RESULT_VOLUME.volume_urn.value)
        self.urn.add(base)
        self.urn.add(path)

        if navigatable:
            path = [ x for x in path.split("/") if x ]
            RESULT_VOLUME.path_cache.add_path_relations(path)

        self.set(pyaff4.AFF4_STORED, RESULT_VOLUME.volume_urn)
        self.proxied = self.proxied.finish()

    def close(self):
        RESULT_VOLUME.update_tables(self.urn)
        self.proxied.close()

class PyFlagStream(PyFlagMap):
    pyaff4_class = pyaff4.Image

class PyFlagGraph(PyFlagMap):
    pyaff4_class = pyaff4.Graph

## These classes are used to manage and create volume features (such
## as navigation).
class PathManager(Store.FastStore):
    def __init__(self, navigation_graph_urn, *args, **kwargs):
        self.URL = pyaff4.RDFURN()
        self.STR = pyaff4.XSDString()
        self.navigation_graph_urn = navigation_graph_urn
        Store.FastStore.__init__(self, *args, **kwargs)

    def add_path_relations(self, path):
        """ Adds navigation relations for path which is a list of components """
        graph = oracle.open(self.navigation_graph_urn, 'w')
        try:
            for i in range(len(path)-1):
                so_far = "/".join(path[:i])
                try:
                    children = self.get(so_far)
                    if path[i] in children:
                        continue

                except KeyError:
                    children = set()
                    self.add(so_far, children)

                children.add(path[i])

                self.URL.set(pyaff4.AFF4_NAVIGATION_ROOT)
                self.URL.add(so_far)

                self.STR.set(path[i])

                ## Add the navigation relation to the graph
                graph.set_triple(self.URL, pyaff4.AFF4_NAVIGATION_CHILD, self.STR)
        finally:
            graph.cache_return()

class ResultVolume:
    """ This class manages the output volume URN where new objects get appended.

    Most of the PyFlag functionality relies on this object.
    """
    def __init__(self):
        """ Given an output volume URN or a path we create this for writing.
        """
        ## A Cache of RDFValue types that aviods us having to recreate
        ## them all the time
        self.rdftypes = {}

        ## A cache of all schema objects keyed by name
        self.schema = {}

        ## A list of all active tables.
        self.tables = []

        self.volume_urn = pyaff4.RDFURN()
        self.volume_urn.set(config.RESULTDIR)
        self.volume_urn.add("Output.aff4")

        ## Try to append to an existing volume
        if not oracle.load(self.volume_urn):
            ## Nope just make it then
            volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
            volume.set(pyaff4.AFF4_STORED, self.volume_urn)

            volume = volume.finish()
            self.volume_urn = volume.urn
            volume.cache_return()

        ## Now make the navigation graph
        graph = pyaff4.Graph(mode = 'w')
        graph.urn.set(self.volume_urn.value)
        graph.urn.add("pyflag/navigation")

        graph.set(pyaff4.AFF4_STORED, self.volume_urn)
        self.navigation_graph_urn = graph.urn
        self.path_cache = PathManager(self.navigation_graph_urn)
        graph = graph.finish()
        graph.cache_return()

        ## Make the metadata graph
        self.make_graph("metadata")
        self.make_graph("schema")

    def make_graph(self, name):
        graph = pyaff4.Graph(mode = 'w')
        graph.urn.set(self.volume_urn.value)
        graph.urn.add("pyflag/%s" % name)
        graph.set(pyaff4.AFF4_STORED, self.volume_urn)
        graph = graph.finish()

        setattr(self, "%s_graph" % name, graph)

    def Seal_output_volume(self):
        self.metadata_graph.close()
        self.schema_graph.close()

        graph = oracle.open(self.navigation_graph_urn, 'w')
        graph.close()

        volume = oracle.open(self.volume_urn, 'w')
        volume.close()

    def VFSCreate(self, fd, name, type=pyaff4.AFF4_MAP):
        """ Creates a new map object based on fd with a name specified """
        obj = oracle.create(type)
        obj.urn.set(fd.urn.value)
        obj.urn.add(name)

        path = [ x for x in obj.urn.parser.query.split("/") if x ]

    def set_metadata(self, urn, attribute, value, graph = 'metadata',
                     rdftype = None):
        if isinstance(value, pyaff4.RDFValue):
            t = value
        ## For convenience allow a string to be directly set
        else:
            if not rdftype:
                if type(value)==str:
                    rdftype = pyaff4.XSDString
                elif type(value)==long:
                    rdftype = pyaff4.XSDInteger
                else:
                    pdb.set_trace()
                    raise RuntimeError("Not sure what RDF type to use for %r" % value)

            ## Maintain a cache of RDF dataTypes:
            try:
                t = self.rdftypes[rdftype]
            except KeyError:
                t = self.rdftypes[rdftype] = rdftype()

            t.set(value)

        graph = getattr(self, "%s_graph" % graph)
        graph.set_triple(urn, attribute, t)

    def add_column(self, class_ref, name):
        try:
            return self.schema[name]
        except KeyError:
            ## Add the column to the current volume
            column = oracle.create(class_ref.dataType)
            column.urn.set(self.volume_urn.value)
            column.urn.add(name)
            column = column.finish()
            self.schema[name] = column.urn
            column.close()

            return self.schema[name]

    def add_table(self, name, columns):
        try:
            return self.schema[name]
        except KeyError:
            table = oracle.create(PYFLAG_TABLE)
            table.urn.set(self.volume_urn.value)
            table.urn.add(name)
            table = table.finish()
            table.columns.extend(columns)
            self.schema[name] = table.urn
            self.tables.append(table.urn)

            table.close()

            return self.schema[name]

    def update_tables(self, urn):
        """ This function is called whenever an AFF4 stream is closed.

        We basically iterate over all the tables in the volume and
        update them with attributes for the new object. This allows
        the SQL tables to be synced up with the RDF data.
        """
        for table_urn in self.tables:
            table = oracle.open(table_urn, 'r')
            try:
                table.add_object(urn)
            finally: table.cache_return()
        #print "%s is closing"% urn.value

## A global pointer to the currently opened volume where results will
## be stored in.
RESULT_VOLUME = None

## Classes to manage HTTP requests and various support functions.
class Query:
    """ A generic wrapper for holding CGI parameters.

    This is almost like a dictionary, except that there are methods
    provided to give access to CGI arrays obtained by repeated use of
    the same key mutiple times.

    @note: This property necessitate the sometime unituitive way of
    resetting a paramter by initially deleting it. For example, to
    change the 'report' parameter in query you must do:

    >>> del query['report']
    >>> query['report'] = 'newvalue'

    since if the paramter is not deleted first, it will simply be
    appended to produce a report array.
    """
    def __init__(self,query_list=(),string=None, user=None, passwd=None, base='',**params):
        """ Constructor initialises from a CGI list of (key,value)
        pairs or named keywords. These may repeat as needed.

        @arg query_list: A list of lists as obtained from cgi.parse_qsl. This way of initialising query_type is obsolete - do not use.

        @arg user: The username or None if unauthenticated.
        @arg passwd: The password used or None if unauthenticated.
        @arg base: The base of the query. This is the part of the URL before the ?.
        """
        # Authentication Stuff
        self.user = user
        self.passwd = passwd
        self.base= base

        ## The window we came from (This is used by HTML to work our
        ## where we need to be drawn to.
        self.window = "window"
        self.callback =''

        if string:
            query_list = cgi.parse_qsl(string)

        self.q=[]
        if isinstance(query_list,list):
            self.q = query_list
        elif isinstance(query_list,tuple):
            self.q = list(query_list)
        elif isinstance(query_list,dict):
            for k,v in query_list.items():
                self.__setitem__(k,v)

        if params:
            for k,v in params.items():
                self.__setitem__(k,v)

        ## Make sure our query is parsed into unicode if needed:
        for i in range(len(self.q)):
            self.q[i] = (smart_unicode(self.q[i][0]), smart_unicode(self.q[i][1]))

    def __str__(self):
        """ Prints the query object as a url string. We encode ourself
        into utf8 to cater for any unicode present.

        NOTE: A URI is build by joining all keys and values with
        &. Both keys and values are properly escaped.
        """
        mark=''
        tmp = self.clone()
        result = []

        for k in tmp.keys():
            if k.startswith("__"):
                del tmp[k]

        for k,v in tmp.q:
            result.append("%s=%s" % (escape_unicode_string(k), escape_unicode_string(v)))

        return "&".join(result)

    def __repr__(self):
        result = ''
        for k,v in self.q:
            result += "%r: %r\n" % (k,v)

        return result

    def __delitem__(self,item):
        """ Removes all instance of item from the CGI object """
        to_remove=[ d for d in self.q if d[0] == item ]
        for i in to_remove:
            try:
                while 1:
                    self.q.remove(i)
            except ValueError:
                pass

    def clear(self, key):
        try:
            del self[key]
        except KeyError:
            pass

    def set(self, key, value):
        del self[key]
        self[key]=smart_unicode(value)

    def default(self, key, value):
        """ Set key to value only if key is not yet defined """
        if not self.has_key(key):
            self[key] = value

    def remove(self,key,value):
        """ Removes the specific instance of key,value from the query.
        @note: Normally you can just do del query['key'], but this will delete all keys,value pairs with the same keys. This is a more finer level method allowing to delete just a single element from the array.
        @except: This will raise a Value Error if the key,value pair do not exist in the query
        """
        index=self.q.index((key,value))
        del self.q[index]

    def keys(self,**options):
        """ Returns a list of all the keys in the query.

        @note: The default behaviour is to return only unique keys, however if the option multiple=True is given, all keys are provided
        """
        if options.has_key('multiple'):
            tmp =[ i[0] for i in self.q ]
        else:
            tmp=[]
            for i in self.q:
                if i[0] not in tmp:
                    tmp.append(i[0])

        return tmp

    def clone(self):
        """ Clones the current object into a new object. Faster than copy.deepcopy """
        tmp = self.__class__(())
        tmp.q = self.q[:]
        tmp.window = self.window
        return tmp

    def getarray(self,key):
        """ Returns an array of all values of key.

        @arg key: CGI key to search for.
        @return: A list of all values (in no particular order). """
        tmp = []
        for i in self.q:
            if i[0] == key:
                tmp.append(i[1])

        return tuple(tmp)

    def poparray(self,key):
        """ Remove last member of array from query """
        tmp = [ i for i in self.q if i[0]==key ]
        try:
            self.remove(tmp[-1][0],tmp[-1][1])
        except IndexError:
            return None
            #raise KeyError

        return tmp[-1][1]

    def has_key(self,key):
        for i in self.q:
            if i[0] == key:
                return True

        return False

    def items(self):
        return self.q.__iter__()

    def __setitem__(self,i,x):
        ## case may only be specified once:
        if i=='case' and self.has_key('case'):
            self.__delitem__('case')

        self.q.append((i,smart_unicode(x)))

    def __getitem__(self,item):
        """ Implements the dictionary access method """
        for i in self.q:
            if i[0]== item:
                return i[1]

        raise KeyError, ("Key '%s' not found in CGI query" % item)

    def get(self,item,default=None):
        for i in self.q:
            if i[0]== item:
                return i[1]

        return default

    def __iter__(self):
        self.iter_count = 0
        return self

    def next(self):
        """ This is used to implement an iterator over the query type. You can now do for x,y in a: """
        try:
            result = self.q[self.iter_count]
        except IndexError:
            raise StopIteration

        self.iter_count+=1
        return result

    def extend(self,dict):
        for k in dict.keys():
            self[k]=dict[k]

    def FillQueryTarget(self,dest):
        """ Given a target, this function returns a updates the query object with a filled in target

        @except KeyError: if the query is not formatted properly (i.e. no _target_ key)
        """
        for target in self.getarray('__target__'):
            ## Calculate the destination value:
            dest = self.get('__target_format__','%s') % dest

            ## Do we need to append it:
            if self.has_key('__target_type__') and self['__target_type__'] == 'append':
                self[target] = dest
            else:
                self.set(target,dest)

## The following functions are for unicode support and are mostly
## borrowed from django:
def smart_unicode(s, encoding='utf-8', errors='ignore'):
    """
    Returns a unicode object representing 's'. Treats bytestrings using the
    'encoding' codec.
    """
    if not isinstance(s, basestring,):
        if hasattr(s, '__unicode__'):
            s = unicode(s)
        else:
            s = unicode(str(s), encoding, errors)
    elif not isinstance(s, unicode):
        try:
            s = s.decode(encoding, errors)
        except:
            s = s.decode('utf8', errors)
            
    return s

def smart_str(s, encoding='utf-8', errors='strict'):
    """
    Returns a bytestring version of 's', encoded as specified in 'encoding'.
    """
    if not isinstance(s, basestring):
        try:
            return str(s)
        except UnicodeEncodeError:
            return unicode(s).encode(encoding, errors)
    elif isinstance(s, unicode):
        return s.encode(encoding, errors)
    elif s and encoding != 'utf-8':
        return s.decode('utf-8', errors).encode(encoding, errors)
    else:
        return s

import urllib
def iri_to_inline_js(iri):
    """ This converts an IRI to a form which can be included within
    inline javascript. For example:

    <a href='

    """
    result = iri_to_uri(iri).replace("%","%25")
    return result

def escape_unicode_string(string):
    """ Returns a quoted unicode string """
    return urllib.quote(smart_str(string), safe='')

def iri_to_uri(iri):
    """
    Convert an Internationalized Resource Identifier (IRI) portion to a URI
    portion that is suitable for inclusion in a URL.

    This is the algorithm from section 3.1 of RFC 3987.  However, since we are
    assuming input is either UTF-8 or unicode already, we can simplify things a
    little from the full method.

    Returns an ASCII string containing the encoded result.
    """
    # The list of safe characters here is constructed from the printable ASCII
    # characters that are not explicitly excluded by the list at the end of
    # section 3.1 of RFC 3987.
    if iri:
        return urllib.quote(smart_str(iri), safe='/%[]=:;$&()+,!?*# ')

def pyflag_escape_string(string):
    result = []
    for x in string:
        if x.isalnum():
            result.append(x)
        else:
            result.append("$%02X" % ord(x))

    return ''.join(result)

def escape(result, quote='\''):
    result = result.replace("\\","\\\\")
    for q in quote:
        result = result.replace(q,'\\'+q)

    result = result.replace("\n","\\n")
    result = result.replace("\r","\\r")
    result = result.replace("\t","\\t")
    result = result.replace("\x00","\\0")

    return result


expand_re = re.compile("%(r|s|b)")

def expand(format, params):
    """ This version of the expand function returns a unicode object
    after properly expanding the params into it.

    NOTE The Python % operator is evil - do not use it because it
    forces the decoding of its parameters into unicode when any of the
    args are unicode (it upcasts them). This is a very unsafe
    operation to automatically do because the default codec is ascii
    which will raise when a string contains a non ascii char. This is
    basically a time bomb which will blow when you least expect it.

    Never use %, use this function instead. This function will return
    a unicode object.
    """
    format = unicode(format)
    d = dict(count = 0)

    if isinstance(params, basestring):
        params = (params,)

    try:
        params[0]
    except:
        params = (params,)

    def cb(m):
        x = params[d['count']]

        if m.group(1)=="s":
            result = u"%s" % (smart_unicode(params[d['count']]))

        ## Raw escaping
        elif m.group(1)=='r':
            result = u"'%s'" % escape(smart_unicode(params[d['count']]), quote="'")

        d['count'] +=1
        return result

    result = expand_re.sub(cb,format)

    return result


def get_bt_string(e=None):
    import sys
    import traceback
    import cStringIO
    
    a = cStringIO.StringIO()
    traceback.print_tb(sys.exc_info()[2], file=a)
    a.seek(0)
    result = a.read()
    a.close()
    return result

def print_bt_string():
    print get_bt_string()

def get_traceback(e,result):
    result.heading("%s: %s" % (sys.exc_info()[0],sys.exc_info()[1]))
    result.text(get_bt_string(e))

import posixpath

def normpath(string):
    """A sane implementation of normpath.

    The Python normpath has a bug whereby it swallaws the last / in a path name - this makes it difficult to distinguish between a directory and a filename.
    This is a workaround this braindead implementation.
    """
    tmp = posixpath.normpath('////'+string)
    if string.endswith('/') and not tmp.endswith('/'):
        tmp=tmp+'/'
    return tmp

def splitpath(path):
    """ Returns all the elements in path as a list """
    path=normpath(path)
    return [ x for x in path.split('/') if x ]

def joinpath(branch):
    return '/'+'/'.join(branch)

def sane_join(*branch):
    return os.path.normpath(os.path.sep.join(branch))

def urlencode(string):
    """ Utility function used for encoding strings inside URLs.

    Replaces non-alphnumeric chars with their % representation

    Note: this could be replaced by urlllib.quote(string)

    """
    result = ''
    for c in "%s" % string:
        if not c.isalnum() and c not in "/.":
            result +="%%%02X" % ord(c)
        else:
            result += c

    return result

def make_tld(host):
    """ A utility function to derive a top level domain from a host name """
    domains = host.lower().split(".")
    tlds = "com.gov.edu.co.go.org.net".split('.')
    for t in tlds:
        try:
            i = domains.index(t)
            return '.'.join(domains[i-1:])
        except ValueError: pass

    return '.'.join(domains[-2:])
