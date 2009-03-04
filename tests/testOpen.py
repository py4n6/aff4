import fif
import unittest

class TestOpen(unittest.TestCase):

    def setUp(self):
        pass

    def testOpenLocalImage(self):
        fiffile = fif.FIFFile("../samples/mediumimage.00.zip")
        fd = fiffile.open_stream_by_name("mediumimage.dd")
        self.assertNotEqual(fd, None)

    def testOpenLocalImageWithBadNameFails(self):
        fiffile = fif.FIFFile("../samples/mediumimage.00.zip")
        self.assertRaises(IOError, fiffile.open_stream_by_name, "badname.dd")
        
    def testOpenLocalImageWithBadFileNameFails(self):
        self.assertRaises(IOError, fif.FIFFile, "../samples/nonexistentimage.00.zip")
      
       
        
def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(TestOpen)
    return suite


if __name__ == '__main__':
    suite = unittest.TestLoader().loadTestsFromTestCase(TestOpen)
    unittest.TextTestRunner(verbosity=2).run(suite)

