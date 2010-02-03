import pyflag.Scanner as Scanner
import re

REQUEST_RE = re.compile("[A-Z]+ [^ ]{1,600} HTTP/1.[01]\r\n")

class HTTPScanner(Scanner.BaseScanner):
    """ Scan for HTTP connections """
    def scan(self, buffer, fd, volume, scanners):
        if REQUEST_RE.search(buffer):
            self.process(fd, volume, scanners)

    def process(self, fd, volume, scanners):
        print "Opening %s as a HTTP Request" % fd.urn.parser.query

