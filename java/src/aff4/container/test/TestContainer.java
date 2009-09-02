package aff4.container.test;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.text.ParseException;

import org.bouncycastle.crypto.digests.MD5Digest;
import org.bouncycastle.util.encoders.Hex;

import junit.framework.TestCase;
import aff4.commonobjects.AuthorityWriter;
import aff4.commonobjects.LinkWriter;
import aff4.commonobjects.WarrantReader;
import aff4.commonobjects.WarrantWriter;
import aff4.container.Container;
import aff4.container.GeneralReadDevice;
import aff4.container.WritableStore;
import aff4.datamodel.AddressResolutionException;
import aff4.datamodel.DiscontiguousMapWriter;
import aff4.datamodel.Reader;
import aff4.datamodel.Writer;


import aff4.infomodel.Literal;
import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QueryTools;
import aff4.infomodel.Resource;
import aff4.infomodel.lexicon.AFF4;
import aff4.storage.zip.AFF4SegmentOutputStream;
import aff4.storage.zip.ReadOnlyZipVolume;
import aff4.storage.zip.StreamReader;
import aff4.storage.zip.StreamWriter;
import aff4.storage.zip.WritableZipVolume;
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

	static ByteBuffer getTestData(String str) {
		ByteBuffer data = ByteBuffer.allocate(32*1024);
			for (int j=0 ; j < 32*1024/str.getBytes().length ; j++) {
				data.put(str.getBytes());
			}
			

		data.limit(data.capacity());
		data.position(0);
		return data;
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
	
	static ByteBuffer a = getTestData("a");
	static ByteBuffer b = getTestData("b");
	
	public void testCreateCorrectInformation() throws ParseException {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolume zv = new WritableZipVolume(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());

			for (int i=0 ; i < 2048 ; i++){
				fd.write(getTestData());
			}
			fd.flush();
			fd.close();
			zv.close();
			
			ReadOnlyZipVolume rzv = new ReadOnlyZipVolume(name);
			QuadList quads = rzv.query(null, null, null, null);
			assertEquals(12, quads.size());
			rzv.close();
		} catch (IOException e) {
			fail();
		}
	}
	
	public void testCreateImage2ChunkBigWrite() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolume zv = new WritableZipVolume(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
			Resource urn = fd.getURN();
			
			ByteBuffer buf = ByteBuffer.allocate(64*1024);
			buf.put(a);
			buf.put(b);
			
			fd.write(buf);

			fd.flush();
			fd.close();
			zv.close();
			
			ReadOnlyZipVolume rzv = new ReadOnlyZipVolume(name);
			Reader dev = new GeneralReadDevice(rzv,urn);
			ByteBuffer readbuf = ByteBuffer.allocate(getTestData().limit());
			int read = dev.read(readbuf);
			assertEquals(getTestData().limit(), read);
			a.clear();
			assertEquals(0, readbuf.compareTo(a));
			readbuf.clear();
			read = dev.read(readbuf);
			assertNotNull(readbuf);
			b.clear();
			assertEquals(0, readbuf.compareTo(b));
			dev.close();
			rzv.close();
			
		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			// TODO Auto-generated catch block
			fail();
		}
	}
	
	public void testCreateImage2ChunkBigWriteAndRead() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolume zv = new WritableZipVolume(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
			Resource urn = fd.getURN();
			
			ByteBuffer buf = ByteBuffer.allocate(64*1024);
			buf.put(a);
			buf.put(b);
			
			fd.write(buf);

			fd.flush();
			fd.close();
			zv.close();
			
			ReadOnlyZipVolume rzv = new ReadOnlyZipVolume(name);
			Reader dev = new GeneralReadDevice(rzv,urn);
			a.clear();
			ByteBuffer readbuf = ByteBuffer.allocate(a.limit()*2);
			int read = dev.read(readbuf);
			a.clear();
			assertEquals(a.limit()*2, read);
			assertEquals(0, readbuf.compareTo(buf));
			dev.close();
			rzv.close();
			
		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			// TODO Auto-generated catch block
			fail();
		} 
	}
	
	public void testCreateImage2ChunkBigWriteAndReadSmall() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolume zv = new WritableZipVolume(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
			Resource urn = fd.getURN();
			
			ByteBuffer buf = ByteBuffer.allocate(64*1024);
			buf.put(a);
			buf.put(b);
			
			fd.write(buf);

			fd.flush();
			fd.close();
			zv.close();
			
			ReadOnlyZipVolume rzv = new ReadOnlyZipVolume(name);
			Reader dev = new GeneralReadDevice(rzv,urn);
			a.clear();
			ByteBuffer readbuf = ByteBuffer.allocate(16*1024);
			int read = dev.read(readbuf);
			assertEquals(16*1024, read);
			read = dev.read(readbuf);
			assertEquals(16*1024, read);
			read = dev.read(readbuf);
			assertEquals(16*1024, read);
			read = dev.read(readbuf);
			assertEquals(16*1024, read);
			read = dev.read(readbuf);
			assertEquals(-1, read);
			dev.close();
			rzv.close();
			
		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			// TODO Auto-generated catch block
			fail();
		} 
	}
	public void testCreateImage2ChunkBigWriteAndReadSeek() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolume zv = new WritableZipVolume(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
			Resource urn = fd.getURN();
			
			ByteBuffer buf = ByteBuffer.allocate(64*1024);
			a.rewind();
			b.rewind();
			buf.put(a);
			buf.put(b);
			
			fd.write(buf);

			fd.flush();
			fd.close();
			zv.close();
			
			ReadOnlyZipVolume rzv = new ReadOnlyZipVolume(name);
			Reader dev = new GeneralReadDevice(rzv,urn);
			ByteBuffer readbuf = ByteBuffer.allocate(getTestData().limit()*2);
			dev.position(a.limit());
			int read = dev.read(readbuf);
			assertEquals(b.limit(), read);
			b.rewind();
			System.out.println(HexFormatter.convertBytesToString(readbuf.array()));
			assertEquals(0, readbuf.compareTo(b));
			
			dev.close();
			rzv.close();
			
		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			// TODO Auto-generated catch block
			fail();
		}
	}
	public void testCreateImageWithDifferentBlocksThenReload() {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			WritableZipVolume zv = new WritableZipVolume(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
			fd.setChunksInSegment(2);
			Resource urn = fd.getURN();

			ByteBuffer c =  getTestData("c");
			ByteBuffer d =  getTestData("d");
			a.rewind();
			b.rewind();
			fd.write(a);
			fd.write(b);
			fd.write(c);
			fd.write(d);
			fd.write(ByteBuffer.wrap(new String("a few more bytes").getBytes()));
			fd.flush();
			fd.close();
			String storedHash = ((Literal)zv.query(Node.ANY, fd.getURN(), AFF4.hash, null).get(0).getObject()).asString();
			zv.close();
			
			MD5Digest hash = new MD5Digest();
			byte[] hasha = new byte[hash.getDigestSize()];
			byte[] hashb = new byte[hash.getDigestSize()];
			byte[] hashc = new byte[hash.getDigestSize()];
			byte[] hashd = new byte[hash.getDigestSize()];
			
			hash.update(a.array(), 0, a.limit());
			hash.doFinal(hasha, 0);
			hash.update(b.array(), 0, b.limit());
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
			
			ByteBuffer readBuffer = ByteBuffer.allocate(32*1024);
			int res = dev.read(readBuffer);
			assertNotNull(readBuffer);
			assertEquals(32*1024, readBuffer.limit());
			hash.update(readBuffer.array(),0,readBuffer.limit());
			hash.doFinal(hashBytes, 0);
			assertTrue(hashesEqual(hasha, hashBytes));
			
			readBuffer.clear();
			res = dev.read(readBuffer);
			assertNotNull(readBuffer);
			assertEquals(32*1024, readBuffer.limit());
			hash.update(readBuffer.array(),0,readBuffer.limit());
			hash.doFinal(hashBytes, 0);
			assertTrue(hashesEqual(hashb, hashBytes));		
			
			readBuffer.clear();
			res = dev.read(readBuffer);
			assertNotNull(readBuffer);
			assertEquals(32*1024, readBuffer.limit());
			hash.update(readBuffer.array(),0,readBuffer.limit());
			hash.doFinal(hashBytes, 0);
			assertTrue(hashesEqual(hashc, hashBytes));		
			
			readBuffer.clear();
			res = dev.read(readBuffer);
			assertNotNull(readBuffer);
			assertEquals(32*1024, readBuffer.limit());
			hash.update(readBuffer.array(),0,readBuffer.limit());
			hash.doFinal(hashBytes, 0);
			assertTrue(hashesEqual(hashd, hashBytes));			
			
			readBuffer.clear();
			res = dev.read(readBuffer);
			assertNotNull(readBuffer);
			assertEquals(16, readBuffer.limit());
			String out = new String(readBuffer.array(),0,16);
			assertEquals("a few more bytes", out);
			
			readBuffer.clear();
			res = dev.read(readBuffer);
			assertEquals(res, -1);
			dev.close();
			
			
			String readHash = ((Literal)QueryTools.queryValue(rzv, Node.ANY, dev.getURN(), AFF4.hash)).asString();
			assertEquals(storedHash, readHash);
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
			
			WritableZipVolume zv = new WritableZipVolume(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
			fd.setChunksInSegment(2);
			Resource urn = fd.getURN();

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
			ByteBuffer readBuffer = ByteBuffer.allocate(8);
			
			int res = dev.read(readBuffer);
			String out = new String(readBuffer.array());
			assertEquals("abcdefgh", out);
			dev.close();
			
			dev = new StreamReader(rzv,urn);
			dev.setChunksInSegment(2);
			
			readBuffer = ByteBuffer.allocate(32*1024);
			readBuffer.clear();
			res = dev.read(readBuffer);
			assertNotNull(readBuffer);
			assertEquals(32*1024, readBuffer.limit());

			readBuffer.clear();
			res = dev.read(readBuffer);
			assertNotNull(readBuffer);
			assertEquals(32*1024, readBuffer.limit());

			readBuffer.clear();
			res = dev.read(readBuffer);
			assertNotNull(readBuffer);
			assertEquals(32*1024, readBuffer.limit());

			readBuffer.clear();
			res = dev.read(readBuffer);
			assertNotNull(readBuffer);
			assertEquals(32*1024, readBuffer.limit());
			

			readBuffer.clear();
			res = dev.read(readBuffer);
			assertNotNull(readBuffer);
			assertEquals(16, readBuffer.limit());
			out = new String(readBuffer.array(),0,16);
			assertEquals("a few more bytes", out);
			

			readBuffer.clear();
			res = dev.read(readBuffer);
			assertEquals(-1, res);
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
			
			WritableZipVolume zv = new WritableZipVolume(name);
			StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
			Resource urn = fd.getURN();
			
			fd.write(getTestData());
			fd.write(getTestData());
			fd.flush();
			fd.close();
			zv.close();
			
			ReadOnlyZipVolume rzv = new ReadOnlyZipVolume(name);
			Reader dev = new GeneralReadDevice(rzv,urn);
			ByteBuffer b = ByteBuffer.allocate(getTestData().limit());
			int res = dev.read(b);
			res = dev.read(b);
			assertNotNull(b);
			res = dev.read(b);
			assertEquals(-1, res);
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
			
			WritableZipVolume zv = new WritableZipVolume(name);
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
			QuadList res = v.query(Node.ANY, Node.ANY, AFF4.type, AFF4.map);
			for (Quad q : res ) {
				Resource urn = q.getSubject();
				ByteBuffer readBuffer = ByteBuffer.allocate(32*1024);
				Reader dev = v.open(urn);
				int r = dev.read(readBuffer);
				try {
					r = dev.read(readBuffer);
					fail();
				} catch (AddressResolutionException ex) {
					
				}
				dev.position(32*1024*2);
				r = dev.read(readBuffer);
				try {
					r = dev.read(readBuffer);
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
			
			WritableStore zv = new Container(null,name);
			StreamWriter fd =  zv.createWriter();

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
			w.addAssertion(zv.getGraph());
			w.close();
			zv.close();
			
			ReadOnlyZipVolume v = new ReadOnlyZipVolume(name);
			QuadList res = v.query(Node.ANY, Node.ANY, AFF4.type, AFF4.Warrant);
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
			
			Container zv = new Container(null, name);
			StreamWriter fd = zv.createWriter();

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
			w.addAssertion(zv.getGraph());
			w.close();
			
			zv.close();
		} catch (Exception e) {
			fail();
		}
	}
	
	public void testCreateImageBevvyPlusOne() throws ParseException {
		try {
			String tempDir = System.getenv("TEMP");
			String name = tempDir + "\\testWrite.zip";
			
			assertTrue(deleteTestFile());
			
			Container zv = new Container(null, name);
			StreamWriter fd = zv.createWriter();

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
			
			WritableZipVolume zv = new WritableZipVolume(name);
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
			
			WritableZipVolume zv = new WritableZipVolume(name);
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
			Reader dev = new GeneralReadDevice(v,Node.createURI("urn:aff4:da0d1948-846f-491d-8183-34ae691e8293"));
			ByteBuffer buf = ByteBuffer.allocate(10);
				dev.read(buf);
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
			Reader dev = new GeneralReadDevice(v,Node.createURI("urn:aff4:07ae2fc9-2ba7-46b2-930c-a7f2df14c9f4/storage"));
			//ByteBuffer buf = dev.read();
			//System.out.println(HexFormatter.convertBytesToString(buf.array()));
		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			fail();
		}
	}
	

	

}
