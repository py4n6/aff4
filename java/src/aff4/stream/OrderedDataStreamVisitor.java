package aff4.stream;

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;

public interface OrderedDataStreamVisitor {
	public void visit(ByteBuffer data);
	public boolean finish(long offset, long length) throws UnsupportedEncodingException;
}
