import pyaff4

oracle = pyaff4.Resolver()

## Shut up messages
class Renderer:
    def message(self, level, service, subject, message):
        print level, service, subject, message

oracle.set_logger(pyaff4.ProxiedLogger(Renderer()))
