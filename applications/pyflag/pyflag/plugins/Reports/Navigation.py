import Reports
import pyaff4
import pdb
import HTMLUI
from functools import partial
import sys
import conf
config=conf.ConfObject()

oracle = pyaff4.Resolver()

class InformationNotebook:
    """ This class renders information about the node in a notebook """
    def __init__(self, urn, result):
        self.urn = urn

        fd = oracle.open(urn, 'r')
        self.size = fd.size
        fd.cache_return()

        ## We fill in the tabs based on introspection
        names = []
        callbacks = []
        for o in dir(self):
            try:
                x, name = o.split("_")
                assert(x=='notebook')
            except: continue

            names.append(name)
            callbacks.append(getattr(self, o))

        result.notebook(names=names, callbacks=callbacks, pane='pane')

    def notebook_Information(self, query, result):
        result.heading(self.urn.value)

    def notebook_Hexview(self, query, result):
        """ Show the hexdump for the file."""
        match=0
        length=0
        try:
            match=int(query['highlight'])
            length=int(query['length'])
        except:
            pass

        max=config.MAX_DATA_DUMP_SIZE

        def hexdumper(offset,data,result):
            dump = HTMLUI.HexDump(data,result)
            # highlighting (default highlight)
            highlight = [[0, sys.maxint, 'alloc'],]

            dump.dump(base_offset=offset,limit=max,highlight=highlight)

        return self.display_data(query,result, max, hexdumper)

    def notebook_Textview(self, query, result):
        max=config.MAX_DATA_DUMP_SIZE
        def textdumper(offset, data,result):
            result.text(data, font='typewriter', sanitise='full', wrap='full', style='red',
                        max_lines = 500)

        return self.display_data(query,result, max, textdumper)

    def display_data(self, query,result,max, cb):
        """ Displays the data.

        The callback takes care of paging the data from self. The callback cb is used to actually render the data:

        'def cb(offset,data,result)

        offset is the offset in the file where we start, data is the data.
        """
        #Set limits for the dump
        try:
            limit=int(query['hexlimit'])
        except KeyError:
            limit=0

        fd = oracle.open(self.urn, 'r')
        try:
            fd.seek(limit)
            data = fd.read(max+1)
        finally:
            fd.cache_return()

        if (not data or len(data)==0):
            result.text("No Data Available")
        else:
            cb(limit,data,result)

        #Do the navbar
        new_query = query.clone()
        previous=limit-max
        if previous<0:
            if limit>0:
                previous = 0
            else:
                previous=None

        if previous != None:
            new_query.set('hexlimit',0)
            result.toolbar(text="Start", icon="stock_first.png",
                           link = new_query,  pane="self")
        else:
            result.toolbar(text="Start", icon="stock_first_gray.png", pane="self")

        if previous != None:
            new_query.set('hexlimit',previous)
            result.toolbar(text="Previous page", icon="stock_left.png",
                           link = new_query,  pane="self")
        else:
            result.toolbar(text="Previous page", icon="stock_left_gray.png", pane="self")

        next=limit + max

        ## If we did not read a full page, we do not display
        ## the next arrow
        if len(data)>=max:
            new_query.set('hexlimit',next)
            result.toolbar(text="Next page", icon="stock_right.png",
                           link = new_query , pane="self")
        else:
            result.toolbar(text="Next page", icon="stock_right_gray.png", pane="self")

        if len(data)>=max:
            new_query.set('hexlimit',self.size.value - self.size.value % 1024)
            result.toolbar(text="End", icon="stock_last.png",
                           link = new_query , pane="self")
        else:
            result.toolbar(text="End", icon="stock_last_gray.png", pane="self")

        ## Allow the user to skip to a certain page directly:
#        result.toolbar(
#            cb = partial(goto_page_cb, variable='hexlimit'),
#            text="Current Offset %s" % limit,
#            icon="stock_next-page.png", pane="popup"
#            )

        return result


class Navigation(Reports.Report):
    """ Navigate the AFF4 Volume """
    family = "File"
    name = "Navigate"

    def display(self, query, result):
        def tree_cb(path):
            child = pyaff4.XSDString()
            urn = pyaff4.RDFURN()
            urn.set(pyaff4.AFF4_NAVIGATION_ROOT)
            urn.add(path)

            iter = oracle.get_iter(urn, pyaff4.AFF4_NAVIGATION_CHILD)
            while oracle.iter_next(iter, child):
                yield (child.value, child.value, 'branch')

        def pane_cb(path, result):
            urn = pyaff4.RDFURN()

            urn.set(pyaff4.AFF4_NAVIGATION_ROOT)
            urn.add(path)
            if oracle.resolve_value(urn, pyaff4.AFF4_NAVIGATION_LINK, urn):
                InformationNotebook(urn, result)

            return

        result.tree(tree_cb = tree_cb, pane_cb = pane_cb)
