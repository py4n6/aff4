package aff4.datamodel.test;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.text.ParseException;

import aff4.datamodel.ReaderInputStream;

import junit.framework.TestCase;

public class TestDataModel extends TestCase {
	public void testEmptyByteArrayReader() {
		ByteArrayReader reader = new ByteArrayReader("");
		ByteBuffer buf = ByteBuffer.allocate(1);
		assertEquals(0, reader.position());
		try {
			reader.position(1);
			fail();
		} catch (IOException e) {
		}
		try {
			reader.position(-5);
			fail();
		} catch (IOException e) {
		}
		try {
			assertEquals(-1, reader.read(buf));
		} catch (ParseException ex) {
			fail();
		} catch (IOException ex) {
			fail();
		}
		
	}

	public void testOneByteArrayReader() {
		ByteArrayReader reader = new ByteArrayReader("a");
		ByteBuffer buf = ByteBuffer.allocate(1);
		assertEquals(0, reader.position());
		try {
			reader.position(1);
			fail();
		} catch (IOException e) {
			
		}
		try {
			reader.position(0);
			assertEquals(1, reader.read(buf));
			assertEquals("a", new String(buf.array(), "UTF-8"));
		} catch (ParseException ex) {
			fail();
		} catch (IOException ex) {
			fail();
		}
		try {
			buf = ByteBuffer.allocate(2);
			reader.position(0);
			assertEquals(1, reader.read(buf));
			assertEquals(0, buf.position());
		} catch (IOException e) {
		} catch (ParseException e){
		}
		
	}
	
	public void testEmptyBufferArrayReader() {
		ByteArrayReader reader = new ByteArrayReader("aa");
		ByteBuffer buf = ByteBuffer.allocate(0);
		assertEquals(0, reader.position());
		try {
			reader.position(1);
			
		} catch (IOException e) {
			fail();
		}
		try {
			reader.position(0);
			assertEquals(-1, reader.read(buf));
		} catch (ParseException ex) {
			fail();
		} catch (IOException ex) {
			fail();
		}
	}
	public void testReaderInputStream() {
		ByteArrayReader reader = new ByteArrayReader("");
		ReaderInputStream is = new ReaderInputStream(reader);
		try {
			assertEquals(0, is.available());
			assertEquals(-1, is.read());
		} catch (IOException e) {
		}
		
		reader = new ByteArrayReader("a");
		 is = new ReaderInputStream(reader);
		try {
			assertEquals(0, is.available());
			assertEquals(97, is.read());
			assertEquals(-1, is.read());
		} catch (IOException e) {
		}
	}
	
	public void testReaderInputStreamLarge() {
		ByteArrayReader reader = new ByteArrayReader(1024);
		ReaderInputStream is = new ReaderInputStream(reader);
		try {
			assertEquals(0, is.available());
			assertEquals(0, is.read());
			for (int i=0 ; i < 1023; i++) {
				assertEquals(0, is.read());
			}
			assertEquals(-1, is.read());
				
		} catch (IOException e) {
		}
		
		 reader = new ByteArrayReader(1025);
		 is = new ReaderInputStream(reader);
		try {
			assertEquals(0, is.available());
			assertEquals(0, is.read());
			for (int i=0 ; i < 1024; i++) {
				assertEquals(0, is.read());
			}
			assertEquals(-1, is.read());
				
		} catch (IOException e) {
		}
	}
}
