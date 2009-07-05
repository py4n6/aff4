from aff4 import *
import unittest,re

class ResolverTests(unittest.TestCase):
    """ Test the universal resolver """
    def test_00access(self):
        """ Test resolver APIs """
        location = "file://somewhere"
        test_urn = "urn:aff4:testurn"
        test_attr = "aff4:stored"
        oracle.add(test_urn, test_attr, location)
        self.assertEqual( location, oracle.resolve(test_urn, test_attr))

class FileBackedObjectTests(unittest.TestCase):
    """ Test the FileBackedObject """
    def test_01FileBackedObject(self):
        """ Tests the file backed object """
        filename = "file://tests/FileBackedObject_test.txt"
        test_string = "hello world"
        
        fd = FileBackedObject(filename, 'w')
        fd.write(test_string)
        fd.close()
        
        fd = oracle.open(filename,'r')
        try:
            self.assertEqual(fd.read(len(test_string)), test_string)
        finally:
            oracle.cache_return(fd)

class HTTPObject_Test(unittest.TestCase):
    """ Test the webdav HTTP object """
    def test_02_HTTP(self):
        """ Test http object """
        url = "http://127.0.0.1/webdav/foobar.txt"
        message = "hello world"

        fd = HTTPObject(url, 'w')
        fd.write(message)
        fd.close()

        fd = HTTPObject(url, 'r')
        self.assertEqual(fd.read(1000), message)
        self.assertEqual(fd.size, len(message))
        fd.close()

class SparseImageTest(unittest.TestCase):
    """ Test the sparse image facility """
    filename = 'tests/test.zip'

    def setUp(self):
        try:
            os.unlink(self.filename)
        except: pass
        
    def test_01_Sparse(self):
        """ Test creation of sparse image """
        volume = ZipVolume(None, 'w')
        oracle.add(volume.urn, AFF4_STORED, self.filename)
        volume.finish()
        oracle.cache_return(volume)

        ## The backing image provides storage
        image = Image(None, 'w')
        oracle.add(image.urn, AFF4_STORED, volume.urn)
        oracle.set(image.urn, AFF4_COMPRESSION, 0)
        image.finish()
        oracle.cache_return(image)

        ## The Map stream provides direct interface
        map_fd = Map(None, 'w')
        oracle.set(map_fd.urn, AFF4_STORED, volume.urn)
        oracle.set(map_fd.urn, AFF4_TARGET, image.urn)
        map_fd.finish()

        map_fd.add(0, 0, '@')

        ## Now write in a sparse way
        map_fd.write("abc")
        map_fd.write("def")
        
        ## Skip some data
        map_fd.seek(3,1)
        map_fd.write("ghi")
        map_fd.seek(-4,1)
        map_fd.write("foobar")

        map_fd.close()

        image = oracle.open(image.urn, 'w')
        image.close()
        
        volume = oracle.open(volume.urn, 'w')
        volume.close()

class ZipVolumeTest(unittest.TestCase):
    """ Creation and reading of Zip Volumes """
    disabled = True
    
    def test_02_ZipVolume(self):
        """ Test creating of zip files """
        filename = "file://tests/test.zip"
        
        out_fd = ZipVolume(None, "w")
        oracle.add(out_fd.urn, AFF4_STORED, filename)
        out_fd.finish()
        out_fd.writestr("default","Hello world")
        out_fd.close()

    def test_02_ImageWriting(self):
        """ Makes a new volume """
        ResolverTests.filename = filename = "file://tests/test.zip"
        in_filename = "file://output.dd"

        ## Make a new volume
        volume_fd = ZipVolume(None, "w")
        volume_urn = volume_fd.urn
        oracle.add(volume_urn, AFF4_STORED, filename)
        volume_fd.finish()
        oracle.cache_return(volume_fd)

        ## Make an image
        image_fd = Image(None, "w")
        image_urn = image_fd.urn
        oracle.add(image_fd.urn, AFF4_STORED, volume_urn)
        image_fd.finish()
        in_fd = oracle.open(in_filename)
        try:
            while 1:
                data = in_fd.read(10240)
                if not data: break

                image_fd.write(data)
        finally:
            oracle.cache_return(in_fd)
            
        image_fd.close()
        
        ## Make a link
        link = Link(fully_qualified_name("default", volume_urn))
        oracle.set(link.urn, AFF4_STORED, volume_urn)
        oracle.set(link.urn, AFF4_TARGET, image_urn)
        link.finish()
        
        link.close()
        
        volume_fd = oracle.open(volume_urn, 'w')
        volume_fd.close()

    def test_03_ZipVolume(self):
        """ Tests the ZipVolume implementation """
        print "Loading volume"
        z = ZipVolume(None, 'r')
        z.load_from(ResolverTests.filename)

        ## Test the stream implementation
        fd = oracle.open(fully_qualified_name("properties", z.urn))
        try:
            data = z.zf.read("properties")
            self.assertEqual(data,fd.read(1024))
        finally:
            oracle.cache_return(fd)
            
        stream = oracle.open(fully_qualified_name("default", z.urn))
        try:
            fd = open("output.dd")
            while 1:
                data = stream.read(1024)
                data2 = fd.read(1024)
                if not data or not data2: break

                self.assertEqual(data2, data)

            import sk
            stream.seek(0)
            fs = sk.skfs(stream)

            print fs.listdir('/')
        finally:
            oracle.cache_return(stream)

if __name__ == '__main__':
    import optparse

    parser = optparse.OptionParser()
    parser.add_option("-m", "--match", default=None,
                      help = "Match the test name to this RE")
    parser.add_option("-L", "--list", default=False,
                      action='store_true',
                      help = "Just list the test names without running them")

    (options, args) = parser.parse_args()

    if options.match:
        match = re.compile(options.match)
    else:
        match = None
        
    for cls_name in dir():
        if match and not match.search(cls_name): continue
        
        cls = globals()[cls_name]
        try:
            if issubclass(cls, unittest.TestCase):
                try:
                    if cls.disabled: continue
                except AttributeError: pass

                try:
                    doc = cls.__doc__
                except: pass
                if not doc:
                    doc = cls

                if options.list:
                    print "%s: %s" % (cls.__name__, doc)
                    continue

                tmp = "Running tests in %s: %s " % (cls.__name__,doc)
                print "-" * len(tmp)
                print tmp
                print "-" * len(tmp)

                suite = unittest.TestSuite()
                suite.addTest(unittest.makeSuite(cls))
                unittest.TextTestRunner(verbosity=2).run(suite)
        except: pass
