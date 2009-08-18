package aff4.storage.test;

import java.io.IOException;
import java.nio.ByteBuffer;

import aff4.container.LinkWriter;
import aff4.storage.StreamWriter;
import aff4.storage.WritableZipVolumeImpl;
import aff4.storage.Writer;

import junit.framework.TestCase;

public class TestStorage extends TestCase {
	
	public void testCreateImageBevvyPlusOne() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolumeImpl zv = new WritableZipVolumeImpl(name);
			StreamWriter fd = zv.open();

			for (int i=0 ; i < 2049 ; i++){
				fd.write(getTestData());
			}
			fd.flush();
			fd.close();
			LinkWriter l = new LinkWriter(zv, fd, "default");
			l.close();
			zv.close();
		} catch (IOException e) {
			fail();
		}
	}
	
	boolean deleteTestFile() {
		String tempDir = System.getenv("TEMP");
		String name = tempDir + "\\testWrite.zip";
		java.io.File f = new java.io.File(name);
		if (!f.exists()) {
			return true;
		}
		if (f.exists() && f.canWrite()) {
			boolean res = f.delete();
			return res;
		}
		return false;
	}
	
	ByteBuffer testData = null;
	
	ByteBuffer getTestData() {
		if (testData == null) {
			String ss = new String("abcdefgh");
			testData = ByteBuffer.allocate(32*1024);
			for (int j=0 ; j < 32*1024/ss.getBytes().length ; j++) {
				testData.put(ss.getBytes());
			}
			
		}
		return testData;
	}
	
}
