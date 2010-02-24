import pyaff4

t = pyaff4.TDB("/tmp/foobar.tdb")
t.store("hello", "world")

print t.fetch("hello")
