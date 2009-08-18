package aff4.storage;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.ByteBuffer;

public interface Writer {
	public void write(ByteBuffer data) throws FileNotFoundException, IOException;
	public void flush() throws FileNotFoundException, IOException;
	public void close() throws FileNotFoundException, IOException;
	public long position() throws FileNotFoundException, IOException;
	public String getURN();
}
