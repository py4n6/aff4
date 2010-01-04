""" Tests for RDFURN """
import pyaff4,pdb

import time
time.sleep(1)

urn = pyaff4.RDFURN()

urn.set("http://www.google.com/hello/")
urn.add_query("world/../../../fo#obar.txt")
urn.add("food")

print urn.parser.string()
