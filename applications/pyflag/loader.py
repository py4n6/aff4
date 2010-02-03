import pyflag.conf as conf
config = conf.ConfObject()

import pyflag.Registry as Registry
import pyaff4
import pyflag.Framework as Framework
import pyflag.Scanner as Scanner

config.add_option("OUTPUT", default="Output.aff4",
                  help = "AFF4 volume to write results on")

config.add_option("SEAL", default=True, action='store_false',
                  help = "Should we seal the AFF4 volume")

config.parse_options(final = True)

Registry.Init()

## Let the application know we are about to start
Framework.post_event("startup")

oracle = pyaff4.Resolver()

outurn = pyaff4.RDFURN()
outurn.set(config.OUTPUT)

## Try to append to an existing volume
if not oracle.load(outurn):
    ## Nope just make it then
    volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
    volume.set(pyaff4.AFF4_STORED, outurn)

    volume = volume.finish()
    outurn = volume.urn
    volume.cache_return()

## Now make up a list of scanners to use
scanners = [ s() for s in Registry.SCANNERS.classes ]

for file in config.args:
    print "Will load %s" % file
    inurn = pyaff4.RDFURN()
    inurn.set(file)

    Scanner.scan_urn(inurn, outurn, scanners)

## Ok we are about to finish
Framework.post_event("finish", outurn)

## Now seal the volume - this is optional
if config.SEAL:
    volume = oracle.open(outurn, 'w')
    volume.close()

## Ok we are about to finish
Framework.post_event("exit")

