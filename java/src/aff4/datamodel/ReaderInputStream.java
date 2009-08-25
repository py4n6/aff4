package aff4.datamodel;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.text.ParseException;

public class ReaderInputStream extends InputStream {
	Reader reader = null;
	ByteBuffer readCache = null;
	boolean primed = false;
	
	public ReaderInputStream(Reader reader) {
		this.reader = reader;
		readCache = ByteBuffer.allocate(1024);
	}

	public int read() throws IOException {
		if (! primed) {
			try {
				int res = reader.read(readCache);
				if (res == 0 || res == -1) {
					return -1;
				}
				readCache.position(0);
			} catch (ParseException ex) {
				throw new IOException();
			}
			primed = true;
		}
		byte result =  readCache.get();
		if (readCache.position() == readCache.limit()) {
			readCache.clear();
			primed = false;
		}
		return result;
	}

	public int available() throws IOException {
		if (! primed) {
			return 0;
		} else {
			return readCache.limit() - readCache.position();
		}
	}

	public boolean markSupported() {
		return false;
	}



}
