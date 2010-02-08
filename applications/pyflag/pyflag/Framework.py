""" Central file for all the miscelaneous functionality which doesnt
fit anywhere else.
"""
import pyflag.Registry as Registry
import pyaff4

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

def VFSCreate(fd, name, volume_urn, type=pyaff4.AFF4_MAP):
    """ Creates a new map object based on fd with a name specified """
    obj = oracle.create(type)
    obj.urn.set(fd.urn.value)
    obj.urn.add(name)

    obj.set(pyaff4.AFF4_STORED, volume_urn)
    return obj.finish()
