import pyaff4, time

time.sleep(1)

date = pyaff4.XSDDatetime()
date.set(time.time())

print date.serialise()
