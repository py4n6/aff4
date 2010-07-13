""" This is a test suite for pyaff4 """

import pyaff4
import unittest
import pickle
import os, pdb
import zipfile
import StringIO
import sys
import re

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
    variable_filenames = ['information.+', '.+/idx']
    def __init__(self, result):
        self.result = result
        self.variable_filenames_re = [ re.compile(x) for x in self.variable_filenames ]

    def parse_zip(self, data):
        z = zipfile.ZipFile(StringIO.StringIO(data))
        result = {}
        for i in z.infolist():
            result[i.filename] = i

        return result, z

    ## These are the attributes which must be constant between each
    ## member
    attributes = [ 'CRC', 'file_size']

    def skip_file(self, filename):
        for re in self.variable_filenames_re:
            if re.match(filename): return True

    def __eq__(self, other):
        """ By default we just compare the string representation """
        current_files, current_zip = self.parse_zip(self.result)
        expected_files, expected_zip = self.parse_zip(other.result)

        for k, v in current_files.items():
            ## These files are allowed to be different because they
            ## usually contain information which depend on the stream
            ## UUIDs:
            if self.skip_file(k): continue

            expected = expected_files[k]
            for attr in self.attributes:
                attr1 = getattr(v, attr)
                attr2 = getattr(expected, attr)
                #open("/tmp/1.zip",'w').write(self.result)
                #open("/tmp/2.zip",'w').write(other.result)
                assert attr1 == attr2, "Zip files not the same (%s:%s = %s vs %s)" % (
                    k,attr, attr1, attr2)

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

    def make_zip_filename(self):
        return os.tmpnam() + ".zip"

    def record(self):
        filename = pyaff4.RDFURN()
        filename.set(self.make_zip_filename())
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
        """ Test the context's zip file against our current zip file.

        Note that we can not test the volumes for bit equality because
        every volume must be unique.
        """
        current = self.get()
        k = self.__class__.__name__
        expected = context.get(k)

        assert current == expected

class ImageTest(ZipFile):
    """ Test making simple AFF4 image streams """
    sentence = "This is a test"
    repeats = 10000

    def write_data(self, image_fd):
        test_data = ''
        for i in range(0,self.repeats):
            image_fd.write(self.sentence)
            test_data += self.sentence

        return test_data

    def make_new_volume(self):
        self.filename = pyaff4.RDFURN()
        self.filename.set(self.make_zip_filename())
        volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
        volume.set(pyaff4.AFF4_STORED, self.filename)
        volume = volume.finish()

        self.volume_urn = volume.urn
        volume.cache_return()

    def close_volume(self):
        volume = oracle.open(self.volume_urn, 'w')
        volume.close()

    def make_image_stream(self):
        image = oracle.create(pyaff4.AFF4_IMAGE)
        image.urn.set(self.volume_urn.value)
        image.urn.add("%s_stream" % self.__class__.__name__)
        image.set(pyaff4.AFF4_STORED, self.volume_urn)

        image = image.finish()
        self.image_urn = image.urn

        print "Created image %s" % self.image_urn.value

        test_data = self.write_data(image)
        image.close()

        return test_data

    def compare_volumes(self):
        ## Read the entire volume
        fd = oracle.open(self.filename, 'r')
        data = fd.read(fd.size.value)
        fd.close()

        ## Store it for comparison
        self.store(AFF4ZipComparator(data))

    def check_data_content(self, test_data, image_urn):
        ## Now test reading the data (in one big step)
        fd = oracle.open(image_urn, 'r')
        data = fd.read(fd.size.value)
        assert data == test_data
        fd.close()

    def record(self):
        self.make_new_volume()
        test_data = self.make_image_stream()
        self.close_volume()

        self.compare_volumes()
        self.check_data_content(test_data, self.image_urn)

class MapTest(ImageTest):
    """ Test the Map implementation """
    sentence = ''.join([chr(x) for x in range(32,126)])

    def make_map_stream(self):
        ## We make a map of the original stream by only taking the
        ## first 5 characters from each sentence
        map = oracle.create(pyaff4.AFF4_MAP)
        map.urn.set(self.volume_urn.value)
        map.urn.add("%s_map" % self.__class__.__name__)

        map.set(pyaff4.AFF4_STORED, self.volume_urn)

        self.map_urn = map.urn

        map = map.finish()

        stream_offset = 0
        data = ''

        while stream_offset < self.repeats * len(self.sentence):
            ## We test the write_from() interface here.
            map.write_from(self.image_urn, stream_offset, 5)
            #map.add_point(stream_offset, map_offset, self.image_urn.value)
            stream_offset += len(self.sentence)
            data += self.sentence[:5]

        map.close()

        return data

    def record(self):
        self.make_new_volume()
        self.make_image_stream()
        test_data = self.make_map_stream()
        self.close_volume()

        self.compare_volumes()
        self.check_data_content(test_data, self.map_urn)

class CustomLogger(pyaff4.Logger):
    def message(self, level, service, subject, message):
        self.data = "Special logger says: %s, %s, %s, %s" % (level, service, subject, message)
        print self.data

class LoggerTest(TestCase):
    """ Test the custom logger """
    def record(self):
        self.logger = CustomLogger()
        oracle.register_logger(self.logger)

        oracle.log(1, "Python", None, "This is a test")
        self.store(self.logger.data)

        oracle.register_logger(None)

## This is the new result context that we create
context = {}

## Build a list of all test cases to run now
test_cases = []
for cls_name in dir():
    try:
        cls = locals()[cls_name]
        if issubclass(cls, TestCase) and cls!=TestCase:
            test_cases.append(cls(context))
    except (AttributeError,TypeError): pass


import optparse

parser = optparse.OptionParser()
parser.add_option("-s", "--store", default=None,
                  help = "Store results to this file")

parser.add_option('-c', "--compare", default=None,
                  help = 'Compare results from this file')

parser.add_option('-l', '--list', default=None, action='store_true',
                  help = 'List unit tests')

parser.add_option('-t', '--test', default=None,
                  help = 'Just do this test')

(options, args) = parser.parse_args()

if options.list:
    for x in test_cases:
        print "%s:\t%s" % (x.__class__.__name__, x.__doc__)

    sys.exit(0)

if options.test:
    for t in test_cases:
        if t.__class__.__name__ == options.test:
            test_cases = [ t ]
            break

    if len(test_cases) > 1:
        print "Test %s not found" % options.test
        sys.exit(-1)

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

    try:
        for test in test_cases:
            print "Checking %s ..." % test.__class__.__name__,
            res = test.test(result)
            if not res:
                print "OK"
            else:
                print "Failed: %s" % res

    except: pdb.post_mortem()
