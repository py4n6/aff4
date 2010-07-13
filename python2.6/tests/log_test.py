import pyaff4, time
time.sleep(1)

oracle = pyaff4.Resolver()

## Shut up messages
class Renderer(pyaff4.Logger):
    def message(self, level, service, subject, message):
        print "Special logger says: ", level, service, subject, message
        #raise RuntimeError("Woohoo")

oracle.register_logger(Renderer())

oracle.log(1, "Python", None, "This is a test")

oracle.register_logger(None)

oracle.log(1, "Python", None, "This is a test")
