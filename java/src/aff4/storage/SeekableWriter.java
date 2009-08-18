package aff4.storage;

import java.io.IOException;

public interface SeekableWriter extends Writer {
	public void seek(long s) throws IOException;
}
