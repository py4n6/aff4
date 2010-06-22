import pyregfi
import pdb
import time
time.sleep(1)

r = pyregfi.RegistryFile("/var/tmp/uploads/testimages/SYSTEM.reg")
iter = r.get_key("ControlSet001/Control/Class/{4D36E972-E325-11CE-BFC1-08002bE10318}/0001".split("/"))
#iter = r.get_key("ControlSet001/Control/Class".split("/"))
#iter = r.get_key()

for x in iter.list_values():
    print "%s = %s (%s)" % (x.rec.valuename, x.get_value(), x.data.type)

for k in iter:
    print k.keyname
