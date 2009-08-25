package aff4.hash;

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;

import org.bouncycastle.crypto.Digest;
import org.bouncycastle.util.encoders.Hex;

public class HashDigestAdapter extends org.bouncycastle.crypto.digests.SHA256Digest {
	int value;
	ByteBuffer valueBytes; 
	Digest hashAlgorithm;
	
	public HashDigestAdapter(Digest algo) {
		hashAlgorithm = algo;
	}
	
	public void update(ByteBuffer buf) {
		update(buf.array(), buf.position(), buf.limit());
	}
	
	public void update(String buf) throws UnsupportedEncodingException {
		byte[] bytes = buf.getBytes("UTF-8");
		update(bytes, 0, bytes.length );
	}
	
	public ByteBuffer doFinal() {
		valueBytes = ByteBuffer.allocate(getDigestSize());
		value = doFinal(valueBytes.array(),0);
		return valueBytes;
	}
	
	public String getStringValue() throws UnsupportedEncodingException  {
		return new String(Hex.encode(valueBytes.array()),"UTF-8");

	}
	
	public void reset() {
		super.reset();
	}
}
