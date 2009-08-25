package aff4.datamodel;

import java.io.IOException;

public interface SeekableWriter extends Writer {
	public void seek(long s) throws IOException;
}
