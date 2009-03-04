import fif
import unittest, shutil, os
import sk,fif,sys
"""
This test creates a Map stream in its own container, which references
the original image. As per test 2, We use SK to extract a block
allocation list for the file and make a map stream. However teh map stream
is stored in testmap.oo.zip
. We then read the

"""

class TestReferencedMap(unittest.TestCase):
    def setUp(self):
        shutil.copyfile('../samples/ntfs1-gen2.00.zip', './ntfs1-gen2.00.zip')
        IMAGEFILENAME = 'ntfs1-gen2.00.zip'
        
        fiffile = fif.FIFFile([IMAGEFILENAME])
        image = fiffile.open_stream_by_name("./images/ntfs1-gen2.dd")
        fs = sk.skfs(image)
        f = fs.open('/RAW/logfile1.txt')
    
        ## We want to append to the last volume:
        #fiffile.append_volume(FILENAME)
        count = 0

        mapfile = fif.FIFFile()
        new_name = "%s.%02d.zip" % ("referencemap", count)
        mapfile.create_new_volume(new_name)
        ## Create a new Map stream
        new_stream = mapfile.create_stream_for_writing(stream_type = 'aff2-storage:Map',
                                                       target = image.getId())
        new_stream.properties["aff2-storage:name"] = "logfile1.txt"
        mapfile.properties["aff2-storage:containsImage"] = new_stream.getId()
        mapfile.properties["aff2-storage:next_volume"] = "file://%s" % IMAGEFILENAME
        count = 0
        block_size = fs.block_size
        ## Build up the mapping function
        for block in f.blocks():
            new_stream.add_point(count * block_size, block * block_size, 0)
            count += 1

        new_stream.pack()
        f.seek(0,2)
        new_stream.size = f.tell()
        new_stream.save_map()
        new_stream.close()
        mapfile.close()
        f.close()
        fs.close()
        image.close()
        fiffile.close()
        mapfile.close()

    def tearDown(self):
        os.unlink("referencemap.00.zip")
        os.unlink('ntfs1-gen2.00.zip')
        
    def testReadReferencedMap(self):
        ## Now we compare the mapped stream with the stream produced by SK:
        FILENAME = './referencemap.00.zip'
        mapfile = fif.FIFFile([FILENAME], None, False)
        map_logfile_stream = mapfile.open_stream_by_name("logfile1.txt")

        imagefile = fif.FIFFile(['ntfs1-gen2.00.zip'])
        image_stream = imagefile.open_stream_by_name("./images/ntfs1-gen2.dd")
        fs = sk.skfs(image_stream)
        image_sk_file = fs.open('/RAW/logfile1.txt')

        ## Do big reads - sk is very slow with compressed ntfs files
        BLOCKSIZE = 1024*1024
        while 1:
            test_data = map_logfile_stream.read(BLOCKSIZE)
            if len(test_data)==0: break

            data = image_sk_file.read(BLOCKSIZE)
            if data!=test_data:
                print len(data), len(test_data)
            self.assertEqual(data,test_data)
        image_sk_file.close()
        fs.close()
        image_stream.close()
        map_logfile_stream.close()
        mapfile.close()
        imagefile.close()

def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(TestReferencedMap)
    return suite


if __name__ == '__main__':
    suite = unittest.TestLoader().loadTestsFromTestCase(TestReferencedMap)
    unittest.TextTestRunner(verbosity=2).run(suite)


