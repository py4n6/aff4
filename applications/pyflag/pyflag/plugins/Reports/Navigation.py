import Reports
import pyaff4
import pdb

ROOT = "aff4://navigation/root"
oracle = pyaff4.Resolver()

class Navigation(Reports.Report):
    """ Navigate the AFF4 Volume """
    family = "File"
    name = "Navigate"

    def display(self, query, result):
        def tree_cb(path):
            child = pyaff4.XSDString()
            urn = pyaff4.RDFURN()
            urn.set(ROOT)
            urn.add(path)

            iter = oracle.get_iter(urn, pyaff4.AFF4_NAVIGATION_CHILD)
            while oracle.iter_next(iter, child):
                yield (child.value, child.value, 'branch')

        def pane_cb(query, result):
            return

        result.tree(tree_cb = tree_cb, pane_cb = pane_cb)
