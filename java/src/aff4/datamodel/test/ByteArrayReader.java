package aff4.datamodel.test;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.text.ParseException;

import aff4.datamodel.Reader;

public class ByteArrayReader implements Reader {
	byte[] bytes = null;
	long readPtr = -1;
	
	public ByteArrayReader(String content) {
		bytes = content.getBytes();
		readPtr = 0;
	}
	
	public ByteArrayReader(int len) {
		bytes = new byte[len];
		readPtr = 0;
	}
	
	public void close() throws IOException {
		// TODO Auto-generated method stub
		
	}

	public void position(long s) throws IOException {
		if (s >= bytes.length || s < 0) {
			throw new IOException();
		}
		readPtr = s;
	}

	public long position() {
		return readPtr;
	}

	public int read(ByteBuffer buf) throws IOException, ParseException {
		long count = Math.min(bytes.length - readPtr, buf.limit() - buf.position());
		if (count <= 0) {
			return -1;
		}
		int startPosition = buf.position();
		
		buf.put(bytes, (int) readPtr, (int)count);
		buf.limit(buf.position());
		buf.position(startPosition);
		
		readPtr += count;
		return (int)count;
		
	}

}
