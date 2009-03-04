import fif
import unittest

class TestReadImage(unittest.TestCase):

    def setUp(self):
        pass

    def testReadContents(self):
        fiffile = fif.FIFFile("../samples/mediumimage.00.zip")
        fd = fiffile.open_stream_by_name("mediumimage.dd")
        count = 0
        while 1:
            data=fd.read(1024*1024*30)
            if len(data)==0: break

            count+=len(data)
        self.assertEqual(count, 92208)
        fiffile.close()
        
def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(TestReadImage)
    return suite


if __name__ == '__main__':
    suite = unittest.TestLoader().loadTestsFromTestCase(TestReadImage)
    unittest.TextTestRunner(verbosity=2).run(suite)

