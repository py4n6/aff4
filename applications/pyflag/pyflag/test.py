import Framework

import time
time.sleep(1)

m=Framework.PyFlagMap("/hello")

m.foobar = "hi"

print m.urn.value, m.foobar

m.close()
