""" This is a test suite for pyaff4 """

import pyaff4
import unittest
import pickle
import os, pdb
import zipfile
import StringIO

oracle = pyaff4.Resolver()

class TestResult:
    """ This class encapsulates a result and allows comparison with some other object """


class TestCase:
    def __init__(self, context=None):
        """ The context if provided is a dictionary with previously
        calculated test results 
        """
        if context==None:
            context = {}

        self.context = context

    def store(self, result = None, key=None):
        if not key: key = self.__class__.__name__
        self.context[key] = result

    def get(self, key=None):
        if not key: key = self.__class__.__name__
        return self.context.get(key)

    def record(self):
        """ This method is used to run this test and collect results
        to be saved in our context
        """

    def test(self, context):
        """ Run a test case against a context """

class RDFTests(TestCase):
    """ Tests setting, serialising and parsing all the supported RDFValues """
    RDFClasses = [ ('XSDDatetime', pyaff4.XSDDatetime, 1277181726),
                   ('XSDString', pyaff4.XSDString, "hello world"),
                   ('Generic URL', pyaff4.RDFURN, "http://www.google.com"),
                   ('File URL', pyaff4.RDFURN, "/etc/password"),
                   ]

    def record(self):
        for key, obj, value in self.RDFClasses:
            obj = obj()
            obj.set(value)

            ## First serialise this
            serialized = obj.serialise()
            self.store(serialized, key=key)

            ## Now try to parse the serialized version and reserialise it
            obj.parse(serialized)
            serialised_again = obj.serialise()
            self.store(serialised_again, key = key + " parsed")

            assert serialised_again == serialized

    def test(self, context):
        """ Run a test case against a context """
        for k, ob, value in self.RDFClasses:
            v = self.context.get(k)
            expected = context.get(k)
            if not v==expected:
                return "Test {0} failed. Got {1} should be {2}".format(k, v, expected)

class AFF4ZipComparator:
    """ This class compares two AFF4 zip volumes for equivalency.

    This is not really trivial since the volume will have a different
    URL since it must be globally unique so we can not do bitwise
    comparisons.
    """
    def __init__(self, result):
        self.result = result

    def parse_zip(self, data):
        z = zipfile.ZipFile(StringIO.StringIO(data))
        result = {}
        for i in z.infolist():
            result[i.filename] = i

        return result, z

    ## These are the attributes which must be constant between each
    ## member
    attributes = [ 'CRC', 'file_size']

    def __eq__(self, other):
        """ By default we just compare the string representation """
        current_files, current_zip = self.parse_zip(self.result)
        expected_files, expected_zip = self.parse_zip(other.result)

        for k, v in current_files.items():
            ## These files will be different
            if k == 'information.turtle': continue

            expected = expected_files[k]
            for attr in self.attributes:
                assert getattr(v, attr) == getattr(expected,attr), "Zip files not the same (%s:%s)" % (k,attr)

            ## Now check the data is the same
            old_data = current_zip.read(k)
            expected_data = expected_zip.read(k)

            assert old_data == expected_data, "File content are not the same (%s)" % k

        ## If we get here the volumes are the same
        return True

class ZipFile(TestCase):
    """ Make a simple zip file and store some data in it """
    Files = [ ("text.txt", "This is a simple text file", 0),
              ("compressed.txt", "Another compressed file", 8),
              ]

    def record(self):
        filename = pyaff4.RDFURN()
        filename.set(os.tmpnam())
        volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
        volume.set(pyaff4.AFF4_STORED, filename)
        volume = volume.finish()

        for name, data, compression in self.Files:
            volume.writestr(name, data, compression)

        volume.close()

        fd = oracle.open(filename, 'r')
        data = fd.read(fd.size.value)
        fd.close()

        self.store(AFF4ZipComparator(data))

    def test(self, context):
        """ Run a test case against a context """
        current = self.get()
        k = self.__class__.__name__
        expected = context.get(k)
        assert current == expected

## This is the new result context that we create
context = {}

## Build a list of all test cases to run now
test_cases = []
for cls_name in dir():
    try:
        cls = locals()[cls_name]
        if issubclass(cls, TestCase):
            test_cases.append(cls(context))
    except (AttributeError,TypeError): pass


import optparse

parser = optparse.OptionParser()
parser.add_option("-s", "--store", default=None,
                  help = "Store results to this file")

parser.add_option('-c', "--compare", default=None,
                  help = 'Compare results from this file')

(options, args) = parser.parse_args()

## Record all the results now
for test in test_cases:
    test.record()

if options.store:
    fd = open(options.store, 'w')
    fd.write(pickle.dumps(context))
    fd.close()

elif options.compare:
    result = open(options.compare, 'r').read()
    result = pickle.loads(result)

    for test in test_cases:
        print "Checking %s ..." % test,
        res = test.test(result)
        if not res:
            print "OK"
        else:
            print "Failed: %s" % res
