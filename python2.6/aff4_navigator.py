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

def populate_tree(tree, node):
    ## Make sure to clear previous children
    try:
        children = tree.get_children(node)
        for child in children:
            tree.delete(child)
    except: pass

    child = pyaff4.XSDString()
    urn = pyaff4.RDFURN()
    urn.set(tree.set(node, 'url'))

    path = tree.set(node, "path")

    iter = oracle.get_iter(urn, pyaff4.AFF4_NAVIGATION_CHILD)
    while oracle.iter_next(iter, child):
        new_path = path + "/" + child.value
        tree.insert(node, "end", text=child.value,
                    values=[new_path, urn.value + "/" + child.value])

def populate_roots(tree):
    node = tree.insert('', 'end', text='/', values=['/', ROOT])

def update_tree(event):
    tree = event.widget
    populate_tree(tree, tree.focus())

def autoscroll(sbar, first, last):
    """Hide and show scrollbar as needed.
    
    Code from Joe English (JE) at http://wiki.tcl.tk/950"""
    first, last = float(first), float(last)
    if first <= 0 and last >= 1:
        sbar.grid_remove()
    else:
        sbar.grid()
    sbar.set(first, last)

root = Tkinter.Tk()

vsb = ttk.Scrollbar(orient="vertical")
hsb = ttk.Scrollbar(orient="horizontal")

tree = ttk.Treeview(columns=("path", 'url', 'size'),
    displaycolumns="size", yscrollcommand=lambda f, l: autoscroll(vsb, f, l),
    xscrollcommand=lambda f, l:autoscroll(hsb, f, l))

vsb['command'] = tree.yview
hsb['command'] = tree.xview

tree.heading("#0", text="Directory Structure", anchor='w')
tree.heading("size", text="File Size", anchor='w')
tree.column("size", stretch=0, width=100)

populate_roots(tree)
tree.bind('<<TreeviewOpen>>', update_tree)

# Arrange the tree and its scrollbars in the toplevel
tree.grid(column=0, row=0, sticky='nswe')
vsb.grid(column=1, row=0, sticky='ns')
hsb.grid(column=0, row=1, sticky='ew')
root.grid_columnconfigure(0, weight=1)
root.grid_rowconfigure(0, weight=1)

root.mainloop()
