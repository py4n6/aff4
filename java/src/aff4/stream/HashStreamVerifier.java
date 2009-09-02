package aff4.stream;

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;

import org.bouncycastle.crypto.Digest;
import org.bouncycastle.crypto.digests.MD5Digest;
import org.bouncycastle.crypto.digests.SHA256Digest;
import org.bouncycastle.util.encoders.Hex;

import aff4.hash.HashDigestAdapter;
import aff4.infomodel.Resource;

public class HashStreamVerifier implements OrderedDataStreamVisitor{
	HashDigestAdapter hashAlgorithm;
	String hash = null;
	Resource dataObject; 
	
	public HashStreamVerifier(Digest digest, Resource dataObject, String hash) {
		this.hash = hash;
		hashAlgorithm = new HashDigestAdapter(digest);
		this.dataObject = dataObject;
	}
	
	public void visit(ByteBuffer data) {
		hashAlgorithm.update(data);
	}
	
	public boolean finish(long offset, long length) throws UnsupportedEncodingException {
		hashAlgorithm.doFinal();
		if (!hashAlgorithm.getStringValue().equals(hash)) {
			System.out.println("Verify failed for: " + dataObject);
			return false;
		} else {
			//System.out.print(".");
			return true;
		}
	}
}
