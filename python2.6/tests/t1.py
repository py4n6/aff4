import pyaff4, time, sys

import pdb
#pdb.set_trace()
#time.sleep(1)

date = pyaff4.XSDDatetime()
date.set(time.time())

for i in range(1,100):
    date.serialise()

print date.serialise()
