import pyaff4, time
time.sleep(1)

oracle = pyaff4.Resolver()

## Shut up messages
class Renderer:
    def message(self, level, service, subject, message):
        print level, service, subject, message
        #raise RuntimeError("Woohoo")

oracle.set_logger(pyaff4.ProxiedLogger(Renderer()))

oracle.log(1, "Python", None, "This is a test")
