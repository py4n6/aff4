import pyaff4, time

time.sleep(1)

a=pyaff4.AES256Password()

a.set("hello")
print a.encrypt("this is a long word. a long blocksize", 32, 1)
