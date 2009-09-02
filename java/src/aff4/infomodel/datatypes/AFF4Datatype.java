package aff4.infomodel.datatypes;

import aff4.infomodel.Node;
import aff4.infomodel.Resource;
import aff4.infomodel.lexicon.AFF4;

public abstract class AFF4Datatype extends DataType {
	
	public static final DataType sha256 = new SHA256DigestDatatype();
	public static final DataType md5 = new MD5DigestDatatype();
	public static final DataType canonical_sha256 = new CanonicalSHA256DigestDatatype();
	public static final DataType canonical_sha256_rsa = new CanonicalSHA256RSASignatureDatatype();
	
	public AFF4Datatype(String dataType) {

		
		super(dataType);
		// TODO Auto-generated constructor stub
	}
	public Resource getURI() {
		return Node.createURI(AFF4.baseURI + dataType);
	}

}
