import pyflag.conf as conf
config = conf.ConfObject()

import pdb
import pyflag.Registry as Registry
import pyaff4
import pyflag.Framework as Framework
import pyflag.Scanner as Scanner

config.add_option("OUTPUT", default="%s/Output.aff4" % config.RESULTDIR,
                  help = "AFF4 volume to write results on")

config.add_option("SEAL", default=True, action='store_false',
                  help = "Should we seal the AFF4 volume")

config.parse_options(final = True)

Registry.Init()

## Let the application know we are about to start
Framework.post_event("startup")

oracle = pyaff4.Resolver()

Framework.Init_output_volume(config.OUTPUT)

## Now make up a list of scanners to use
scanners = [ s() for s in Registry.SCANNERS.classes ]

for file in config.args:
    print "Will load %s" % file
    inurn = pyaff4.RDFURN()
    inurn.set(file)

    try:
        Scanner.scan_urn(inurn, scanners)
    except:
        pdb.post_mortem()
        raise

## Ok we are about to finish
Framework.post_event("finish")

## Now seal the volume - this is optional
if config.SEAL:
    Framework.Seal_output_volume()

## Ok we are about to finish
Framework.post_event("exit")

