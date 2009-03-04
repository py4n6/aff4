import fif
import unittest, os

class TestOpen(unittest.TestCase):

    def setUp(self):
        # create a basic container for these tests
        fiffile = fif.FIFFile()
        fiffile.create_new_volume("image.zip")
        stream = fiffile.create_stream_for_writing(stream_type = "aff2-storage:Image",
                                                  local_name = "fooimage.dd")

        # create an information storage stream to store the source information
        infostream = fiffile.create_stream_for_writing(stream_type = "aff2-storage:Info")

        stream.properties["aff2-storage:streamSource"] = infostream.getId()
        infostream.properties["aff2-storage:type"] = "aff2-info:File"
        infostream.properties["aff2-storage:path"] = "badpath"
        fiffile.properties["aff2-storage:containsImage"] = stream.getId()

        data = "foobar"
        stream.write(data)
        stream.close()
        infostream.close()
        fiffile.close()

        
    def testOpenLocalImage(self):
        fiffile = fif.FIFFile("image.zip")
        fd = fiffile.open_stream_by_name("fooimage.dd")
        self.assertNotEqual(fd, None)
        fiffile.close()

    def testOpenLocalImageWithBadNameFails(self):
        fiffile = fif.FIFFile("image.zip")
        self.assertRaises(IOError, fiffile.open_stream_by_name, "badname.dd")
        fiffile.close()
        
    def testOpenLocalImageWithBadFileNameFails(self):
        self.assertRaises(IOError, fif.FIFFile, "../samples/nonexistentimage.00.zip")

    def tearDown(self):
        os.unlink("image.zip")
    
        
def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(TestOpen)
    return suite


if __name__ == '__main__':
    suite = unittest.TestLoader().loadTestsFromTestCase(TestOpen)
    unittest.TextTestRunner(verbosity=2).run(suite)

