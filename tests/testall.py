import testReadImageContents, testReferencedMap, testReadMaps, testResolverOpen
import testOpen
import unittest

if __name__ == '__main__':
    suite1 = testResolverOpen.suite()
    suite2 = testOpen.suite()
    suite3 = testReadImageContents.suite()
    suite4 = testReferencedMap.suite()
    suite5 = testReadMaps.suite()
    alltests = unittest.TestSuite([suite1, suite2, suite3, suite4, suite5])
    unittest.TextTestRunner(verbosity=2).run(alltests)
    
