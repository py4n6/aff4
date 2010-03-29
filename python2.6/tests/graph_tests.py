""" This tests the creation of a graph """
import pyaff4, os, time

time.sleep(1)

## We first create a new volume to store it:
urn = pyaff4.RDFURN()
urn.set("/tmp/test.aff4")

try:
    os.unlink(urn.parser.query)
except: pass

oracle = pyaff4.Resolver()

volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
volume.set(pyaff4.AFF4_STORED, urn)
volume = volume.finish()
volume_urn = volume.urn
volume.cache_return()

## Now make a new graph object
graph = oracle.create(pyaff4.AFF4_GRAPH)
graph.set(pyaff4.AFF4_STORED, volume_urn)
graph = graph.finish()

## Just put in some data:
data = pyaff4.XSDString()
data.set("world")
graph.set_triple(volume_urn, "hello", data)

## Close everything.
graph.close()

volume = oracle.open(volume_urn, 'w')
volume.close()
