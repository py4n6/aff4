import pyaff4

import time
time.sleep(1)

import Framework


m=Framework.Column("columns/ip/src_addr")

m.set_metadata('hello',"world")
m.close()
