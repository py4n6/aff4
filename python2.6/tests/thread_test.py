""" This one basically tests that we can call pyaff4 objects from another thread.

This is a standalone to make it easier to run under valgrind.
"""

import pyaff4
import threading


class Logger(pyaff4.Logger):
    def message(self, level, service, subject, message):
        print "%s %s %s" % (service, subject, message)

class Test(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)

        self.resolver = pyaff4.Resolver()
        self.logger = Logger()
        self.resolver.register_logger(self.logger)

    def run(self):
        print "Hello"
        self.resolver.log(1, "foo", None, "hello")

Test().start()
