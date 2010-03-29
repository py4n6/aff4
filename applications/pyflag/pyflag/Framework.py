""" Central file for all the miscelaneous functionality which doesnt
fit anywhere else.
"""
import pyflag.Registry as Registry
import pyaff4
import pdb
import pyflag.Store as Store

class EventHandler:
    """ An event handler object allows plugins to register their
    interst in being notified about sepecific events in the
    application's life.
    """
    def startup(self):
        """ This method is called when we first start """

    def finish(self):
        """ This method is called when we are finished processing """

    def exit(self):
        """ This event happens before we exit """

def post_event(event, *args, **kwargs):
    for e in Registry.EVENT_HANDLERS.classes:
        e = e()
        method = getattr(e, event)
        method(*args, **kwargs)

oracle = pyaff4.Resolver()

class PATH_MANAGER(Store.FastStore):
    def __init__(self, *args, **kwargs):
        self.URL = pyaff4.RDFURN()
        self.STR = pyaff4.XSDString()
        Store.FastStore.__init__(self, *args, **kwargs)

    def add_path_relations(self, path):
        """ Adds navigation relations for path which is a list of components """
        global OUTPUT_VOLUME_URN, OUTPUT_NATIVATION_GRAPH

        graph = oracle.open(OUTPUT_NATIVATION_GRAPH, 'w')
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

PATH_CACHE = PATH_MANAGER()

## This is the output volume URN where new objects get appended
OUTPUT_VOLUME_URN = None
OUTPUT_NATIVATION_GRAPH = None

def Init_output_volume(out_path):
    """ Given an output volume URN or a path we create this for writing.
    """
    global OUTPUT_VOLUME_URN, OUTPUT_NATIVATION_GRAPH

    OUTPUT_VOLUME_URN = pyaff4.RDFURN()
    OUTPUT_VOLUME_URN.set(out_path)

    ## Try to append to an existing volume
    if not oracle.load(OUTPUT_VOLUME_URN):
        ## Nope just make it then
        volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
        volume.set(pyaff4.AFF4_STORED, OUTPUT_VOLUME_URN)

        volume = volume.finish()
        OUTPUT_VOLUME_URN = volume.urn
        volume.cache_return()

        ## Now make the navigation graph
        graph = oracle.create(pyaff4.AFF4_GRAPH)
        graph.urn.set(OUTPUT_VOLUME_URN.value)
        graph.urn.add("pyflag/navigation")

        graph.set(pyaff4.AFF4_STORED, OUTPUT_VOLUME_URN)
        graph = graph.finish()

        OUTPUT_NATIVATION_GRAPH = graph.urn
        graph.cache_return()
    else:
        OUTPUT_NATIVATION_GRAPH = pyaff4.RDFURN()
        OUTPUT_NATIVATION_GRAPH.set(OUTPUT_VOLUME_URN.value)
        OUTPUT_NATIVATION_GRAPH.add("pyflag/navigation")

def Seal_output_volume():
    global OUTPUT_VOLUME_URN, OUTPUT_NATIVATION_GRAPH

    graph = oracle.open(OUTPUT_NATIVATION_GRAPH, 'w')
    graph.close()

    volume = oracle.open(OUTPUT_VOLUME_URN, 'w')
    volume.close()

def VFSCreate(fd, name, type=pyaff4.AFF4_MAP):
    """ Creates a new map object based on fd with a name specified """
    obj = oracle.create(type)
    obj.urn.set(fd.urn.value)
    obj.urn.add(name)

    path = [ x for x in obj.urn.parser.query.split("/") if x ]
    PATH_CACHE.add_path_relations(path)

    obj.set(pyaff4.AFF4_STORED, OUTPUT_VOLUME_URN)
    return obj.finish()
