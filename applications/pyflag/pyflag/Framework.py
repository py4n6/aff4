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

    def finish(self, outurn):
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

    def add_path_relations(self, path, volume_urn):
        """ Adds navigation relations for path which is a list of components """
        for i in range(len(path)-1):
            so_far = "/".join(path[:i])
            if so_far == '': so_far='/'
            try:
                children = self.get(so_far)
                if path[i+1] in children:
                    continue
            except KeyError:
                children = set()
                self.add(so_far, children)

            children.add(path[i+1])

            self.URL.set(pyaff4.AFF4_NAVIGATION_ROOT)
            self.URL.add(so_far[1:])

            self.STR.set(path[i+1])

            oracle.set_value(self.URL, pyaff4.AFF4_NAVIGATION_CHILD, self.STR)
            if not oracle.resolve_value(self.URL, pyaff4.AFF4_TYPE, self.STR):
                self.STR.set(pyaff4.AFF4_GRAPH)
                oracle.set_value(self.URL, pyaff4.AFF4_STORED, self.STR)

                oracle.set_value(self.URL, pyaff4.AFF4_VOLATILE_STORED, volume_urn)

PATH_CACHE = PATH_MANAGER()

def VFSCreate(fd, name, volume_urn, type=pyaff4.AFF4_MAP):
    """ Creates a new map object based on fd with a name specified """
    obj = oracle.create(type)
    obj.urn.set(fd.urn.value)
    obj.urn.add(name)

    PATH_CACHE.add_path_relations(obj.urn.parser.query.split("/"), volume_urn)

    obj.set(pyaff4.AFF4_STORED, volume_urn)
    return obj.finish()
