import fif
import unittest

class TestResolverOpen(unittest.TestCase):

    def setUp(self):
        pass

    def testOpenUnknownFile(self):
        resolver = fif.Resolver()
        res = resolver.resolve("file://unknown.zip")
        self.assertEquals(len(res), 0)
        
    def testOpenLocalImage(self):
        resolver = fif.Resolver()
        res = resolver.resolve("file://../samples/mediumimage.00.zip")
        print res
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0].__class__, fif.FIFFile)

    def testOpenLocalNameInImageYieldsImage(self):
        resolver = fif.Resolver()
        res = resolver.resolve("file://../samples/mediumimage.00.zip/mediumimage.dd")
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0].__class__,fif.Image)

        
        
def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(TestResolverOpen)
    return suite


if __name__ == '__main__':
    suite = unittest.TestLoader().loadTestsFromTestCase(TestResolverOpen)
    unittest.TextTestRunner(verbosity=2).run(suite)
