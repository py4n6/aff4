package aff4.container.test;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.text.ParseException;

import org.bouncycastle.crypto.digests.MD5Digest;
import org.bouncycastle.util.encoders.Hex;

import junit.framework.TestCase;
import aff4.container.AuthorityWriter;
import aff4.container.GeneralReadDevice;
import aff4.container.LinkWriter;
import aff4.container.WarrantReader;
import aff4.container.WarrantWriter;


import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.storage.AddressResolutionException;
import aff4.storage.DiscontiguousMapWriter;
import aff4.storage.StreamReader;
import aff4.storage.StreamWriter;
import aff4.storage.Reader;
import aff4.storage.ReadOnlyZipVolume;
import aff4.storage.WritableZipVolumeImpl;
import aff4.util.HexFormatter;
import de.schlichtherle.io.File;

public class TestContainer extends TestCase {
	
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
		testData.limit(testData.capacity());
		return testData;
	}

	ByteBuffer getTestData(String str) {
		if (testData == null) {

			testData = ByteBuffer.allocate(32*1024);
			for (int j=0 ; j < 32*1024/str.getBytes().length ; j++) {
				testData.put(str.getBytes());
			}
			
		}
		testData.limit(testData.capacity());
		return testData;
	}
	
	boolean hashesEqual(byte[] a, byte[] b) {
		if (a.length != b.length) {
			return false;
		}
		
		for (int i=0 ; i < 16 ; i++) {
			if (a[i] != b[i]) {
				return false;
			}
		}
		return true;
	}
	
	public void testCreateImageWithDifferentBlocksThenReload() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolumeImpl zv = new WritableZipVolumeImpl(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
			fd.setChunksInSegment(2);
			String urn = fd.getURN();

			ByteBuffer a =  getTestData("a");
			ByteBuffer bb =  getTestData("b");
			ByteBuffer c =  getTestData("c");
			ByteBuffer d =  getTestData("d");
			fd.write(a);
			fd.write(bb);
			fd.write(c);
			fd.write(d);
			fd.write(ByteBuffer.wrap(new String("a few more bytes").getBytes()));
			fd.flush();
			fd.close();
			zv.close();
			
			MD5Digest hash = new MD5Digest();
			byte[] hasha = new byte[hash.getDigestSize()];
			byte[] hashb = new byte[hash.getDigestSize()];
			byte[] hashc = new byte[hash.getDigestSize()];
			byte[] hashd = new byte[hash.getDigestSize()];
			
			hash.update(a.array(), 0, a.limit());
			hash.doFinal(hasha, 0);
			hash.update(bb.array(), 0, bb.limit());
			hash.doFinal(hashb, 0);
			hash.update(c.array(), 0, c.limit());
			hash.doFinal(hashc, 0);
			hash.update(d.array(), 0, d.limit());
			hash.doFinal(hashd, 0);
			
			ReadOnlyZipVolume rzv = new ReadOnlyZipVolume(name);

			StreamReader dev = new StreamReader(rzv,urn);
			
			ByteBuffer b;
			dev.setChunksInSegment(2);
			byte[] hashBytes = new byte[16];
			
			b = dev.read(32*1024);
			assertNotNull(b);
			assertEquals(32*1024, b.limit());
			hash.update(b.array(),0,b.limit());
			hash.doFinal(hashBytes, 0);
			assertTrue(hashesEqual(hasha, hashBytes));
			
			b = dev.read(32*1024);
			assertNotNull(b);
			assertEquals(32*1024, b.limit());
			hash.update(b.array(),0,b.limit());
			hash.doFinal(hashBytes, 0);
			assertTrue(hashesEqual(hashb, hashBytes));		
			b = dev.read(32*1024);
			assertNotNull(b);
			assertEquals(32*1024, b.limit());
			hash.update(b.array(),0,b.limit());
			hash.doFinal(hashBytes, 0);
			assertTrue(hashesEqual(hashc, hashBytes));		
			b = dev.read(32*1024);
			assertNotNull(b);
			assertEquals(32*1024, b.limit());
			hash.update(b.array(),0,b.limit());
			hash.doFinal(hashBytes, 0);
			assertTrue(hashesEqual(hashd, hashBytes));			
			b = dev.read(16);
			assertNotNull(b);
			assertEquals(16, b.limit());
			String out = new String(b.array());
			assertEquals("a few more bytes", out);
			
			b = dev.read(1);
			assertNull(b);
			dev.close();

			dev.close();
			rzv.close();

		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			fail();
		}
	}
	
	public void testCreateImageThenReload() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolumeImpl zv = new WritableZipVolumeImpl(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
			fd.setChunksInSegment(2);
			String urn = fd.getURN();

			for (int i=0 ; i < 4 ; i++){
				fd.write(getTestData());
			}
			fd.write(ByteBuffer.wrap(new String("a few more bytes").getBytes()));
			fd.flush();
			fd.close();
			zv.close();
			
			ReadOnlyZipVolume rzv = new ReadOnlyZipVolume(name);

			StreamReader dev = new StreamReader(rzv,urn);
			dev.setChunksInSegment(2);
			ByteBuffer b = dev.read(8);
			String out = new String(b.array());
			assertEquals("abcdefgh", out);
			dev.close();
			
			dev = new StreamReader(rzv,urn);
			dev.setChunksInSegment(2);
			b = dev.read(32*1024);
			assertNotNull(b);
			assertEquals(32*1024, b.limit());
			b = dev.read(32*1024);
			assertNotNull(b);
			assertEquals(32*1024, b.limit());
			b = dev.read(32*1024);
			assertNotNull(b);
			assertEquals(32*1024, b.limit());
			b = dev.read(32*1024);
			assertNotNull(b);
			assertEquals(32*1024, b.limit());
			b = dev.read(16);
			assertNotNull(b);
			assertEquals(16, b.limit());
			out = new String(b.array());
			assertEquals("a few more bytes", out);
			b = dev.read(1);
			assertNull(b);
			dev.close();

			dev.close();
			rzv.close();

		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			fail();
		}
	}
	
	public void testCreateImage2Chunk() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolumeImpl zv = new WritableZipVolumeImpl(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
			String urn = fd.getURN();
			
			fd.write(getTestData());
			fd.write(getTestData());
			fd.flush();
			fd.close();
			zv.close();
			
			ReadOnlyZipVolume rzv = new ReadOnlyZipVolume(name);
			Reader dev = new GeneralReadDevice(rzv,urn);
			ByteBuffer b = dev.read(getTestData().limit());
			b = dev.read(getTestData().limit());
			assertNotNull(b);
			b = dev.read(getTestData().limit());
			assertNull(b);
			dev.close();
			rzv.close();
			
		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			// TODO Auto-generated catch block
			fail();
		}
	}
	
	public void testCreateDiscontigouos() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolumeImpl zv = new WritableZipVolumeImpl(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
			DiscontiguousMapWriter mw = new DiscontiguousMapWriter(zv, fd);
			
			long offset = 0;
			for (int i=0 ; i < 4 ; i++){
				ByteBuffer buf = getTestData();
				int l = buf.limit();
				mw.write(buf);
				offset = offset+ (l*2);
				mw.seek(offset);
			}
			
			mw.close();
			fd.close();
			zv.close();
			

			
			ReadOnlyZipVolume v = new ReadOnlyZipVolume(name);
			QuadList res = v.query(null, null, "aff4:type", "map");
			for (Quad q : res ) {
				String urn = q.getSubject();
				
				Reader dev = v.open(urn);
				ByteBuffer b = dev.read(32*1024);
				try {
					b = dev.read(32*1024);
					fail();
				} catch (AddressResolutionException ex) {
					
				}
				dev.seek(32*1024*2);
				b = dev.read(32*1024);
				try {
					b = dev.read(32*1024);
					fail();
				} catch (AddressResolutionException ex) {
					
				}				

				dev.close();
			}
			v.close();
			
		} catch (Exception e) {
			fail();
		}
	}
	
	public void testCreateSignedAndVerify() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolumeImpl zv = new WritableZipVolumeImpl(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());

			for (int i=0 ; i < 2049 ; i++){
				fd.write(getTestData());
			}
			fd.flush();
			fd.close();
			LinkWriter l = new LinkWriter(zv, fd, "default");
			l.close();
			
			AuthorityWriter a = new AuthorityWriter(zv, "bradley@schatzforensic.com.au");
			a.setPrivateCert("C:\\mysrc\\AFF4.1\\aff4\\python\\key.pem");
			a.setPublicCert("C:\\mysrc\\AFF4.1\\aff4\\python\\cert.pem");
			a.close();
			
			WarrantWriter w = new WarrantWriter(zv);
			w.setAuthority(a);
			w.addAssertion(fd.getURN() + "properties");
			w.close();
			zv.close();
			
			ReadOnlyZipVolume v = new ReadOnlyZipVolume(name);
			QuadList res = v.query(null, null, "aff4:type", "aff4:Warrant");
			for (Quad q : res ) {
				WarrantReader wr = new WarrantReader(v, q.getSubject());
				assertTrue(wr.isValid());
			}
			v.close();
			
		} catch (Exception e) {
			fail();
		}
	}
	
	public void testCreateSigned() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolumeImpl zv = new WritableZipVolumeImpl(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());

			for (int i=0 ; i < 2049 ; i++){
				fd.write(getTestData());
			}
			fd.flush();
			fd.close();
			LinkWriter l = new LinkWriter(zv, fd, "default");
			l.close();
			
			AuthorityWriter a = new AuthorityWriter(zv, "bradley@schatzforensic.com.au");
			a.setPrivateCert("C:\\mysrc\\AFF4.1\\aff4\\python\\key.pem");
			a.setPublicCert("C:\\mysrc\\AFF4.1\\aff4\\python\\cert.pem");
			a.close();
			
			WarrantWriter w = new WarrantWriter(zv);
			w.setAuthority(a);
			w.addAssertion(fd.getURN() + "/properties");
			w.close();
			
			zv.close();
		} catch (Exception e) {
			fail();
		}
	}
	
	public void testCreateImageBevvyPlusOne() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolumeImpl zv = new WritableZipVolumeImpl(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());

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
	

	

	
	public void testCreateImageBevvy() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolumeImpl zv = new WritableZipVolumeImpl(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());

			for (int i=0 ; i < 2048 ; i++){
				fd.write(getTestData());
			}
			fd.flush();
			fd.close();
			zv.close();
		} catch (IOException e) {
			fail();
		}
	}



	
	public void testCreateImage1Chunk() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolumeImpl zv = new WritableZipVolumeImpl(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
			
			fd.write(getTestData());
			fd.flush();
			fd.close();
		} catch (IOException e) {
			fail();
		}
	}
	

	public void testMap() {
		try {
			ReadOnlyZipVolume v = new ReadOnlyZipVolume("C:\\mysrc\\AFF4.1\\aff4\\python\\test.zip");
			Reader dev = new GeneralReadDevice(v,"urn:aff4:da0d1948-846f-491d-8183-34ae691e8293");
			ByteBuffer buf = dev.read(10);
			System.out.println(HexFormatter.convertBytesToString(buf.array()));
		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			fail();
		}
	}
	
	public void testGetComment() {
		File file = new File("C:\\mysrc\\AFF4.1\\aff4\\python\\test.zip");
		
		for (String s : file.list()) {
			System.out.println(s);
		}
	}
	
	public void testZipFile() {
		try {
			ReadOnlyZipVolume v = new ReadOnlyZipVolume("C:\\mysrc\\AFF4.1\\aff4\\python\\test.zip");
			for (Quad q : v.query(null, null, null, null)) {
				System.out.println(q.toString());
			}
			
		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			fail();;
		}
	}
	
	
	public void testContent1() {
		try {
			ReadOnlyZipVolume v = new ReadOnlyZipVolume("C:\\mysrc\\AFF4.1\\aff4\\python\\smallNTFS.zip");
			Reader dev = new GeneralReadDevice(v,"urn:aff4:07ae2fc9-2ba7-46b2-930c-a7f2df14c9f4/storage");
			//ByteBuffer buf = dev.read();
			//System.out.println(HexFormatter.convertBytesToString(buf.array()));
		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			fail();
		}
	}
	

	

}
