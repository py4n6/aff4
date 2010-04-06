"""
This application uses a gui to present the AFF4 navigation space.
"""
import pyaff4, pdb
import os, glob
import ttk

oracle = pyaff4.Resolver()

try:
    import Tkinter
except ImportError:
    import tkinter as Tkinter

ROOT = "aff4://navigation/root"
oracle = pyaff4.Resolver()

class App:
    def __init__(self):
        self.root = Tkinter.Tk()
        vsb = ttk.Scrollbar(orient="vertical")
        hsb = ttk.Scrollbar(orient="horizontal")

        self.tree = ttk.Treeview(
            columns=("path", 'url', 'size'),
            displaycolumns="size", yscrollcommand=lambda f, l: self.autoscroll(vsb, f, l),
            xscrollcommand=lambda f, l: self.autoscroll(hsb, f, l))

        vsb['command'] = self.tree.yview
        hsb['command'] = self.tree.xview

        self.tree.heading("#0", text="Directory Structure", anchor='w')
        self.tree.heading("size", text="File Size", anchor='w')
        self.tree.column("size", stretch=0, width=100)

        ## Populate the root
        self.tree.insert('', 'end', text='/', values=['/', ROOT])

        self.tree.bind('<<TreeviewOpen>>', self.update_tree)

        # Arrange the tree and its scrollbars in the toplevel
        self.tree.grid(column=0, row=0, sticky='nswe')
        vsb.grid(column=1, row=0, sticky='ns')
        hsb.grid(column=0, row=1, sticky='ew')

        self.mainframe = ttk.Frame(self.root, padding="3 3 12 12", width=640, height=480)
        self.mainframe.grid(column = 1, row=0, sticky='nwse')
        self.mainframe.columnconfigure(0, weight=1)
        self.mainframe.rowconfigure(0, weight=1)

        self.frame = ttk.Frame(self.mainframe, width=640, height=480)
        self.frame.pack()

        self.root.grid_columnconfigure(0, weight=1)
        self.root.grid_rowconfigure(0, weight=1)

        self.root.mainloop()

    def render_pane(self, node):
        urn = self.tree.set(node, 'url')

        self.frame.forget()
        self.frame = ttk.Frame(self.mainframe, width=640, height=480)
        label = ttk.Label(self.frame, text=urn, justify = 'center')
        label.pack()
        self.frame.pack()

    def populate_tree(self, node):
        ## Make sure to clear previous children
        try:
            children = self.tree.get_children(node)
            for child in children:
                self.tree.delete(child)
        except: pass

        child = pyaff4.XSDString()
        urn = pyaff4.RDFURN()
        urn.set(self.tree.set(node, 'url'))

        path = self.tree.set(node, "path")

        iter = oracle.get_iter(urn, pyaff4.AFF4_NAVIGATION_CHILD)
        while oracle.iter_next(iter, child):
            new_path = path + "/" + child.value
            self.tree.insert(node, "end", text=child.value,
                             values=[new_path, urn.value + "/" + child.value])

    def update_tree(self, event):
        node = self.tree.focus()
        self.populate_tree(node)
        self.render_pane(node)

    def autoscroll(self, sbar, first, last):
        """Hide and show scrollbar as needed.

        Code from Joe English (JE) at http://wiki.tcl.tk/950"""
        first, last = float(first), float(last)
        if first <= 0 and last >= 1:
            sbar.grid_remove()
        else:
            sbar.grid()
        sbar.set(first, last)

App()
