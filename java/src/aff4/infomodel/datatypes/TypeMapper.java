package aff4.infomodel.datatypes;

import java.util.HashMap;

public class TypeMapper {
	private static TypeMapper instance = null;
	HashMap<String, DataType> map = null;
	
	private TypeMapper() {
		map = new HashMap<String, DataType>();
		map.put(XSDDataType.integer.getURI().getFullURI(), XSDDataType.integer);
		map.put(AFF4Datatype.canonical_sha256.getURI().getFullURI(), AFF4Datatype.canonical_sha256);
		map.put(AFF4Datatype.canonical_sha256_rsa.getURI().getFullURI(), AFF4Datatype.canonical_sha256_rsa);
		map.put(AFF4Datatype.md5.getURI().getFullURI(), AFF4Datatype.md5);
		map.put(AFF4Datatype.sha256.getURI().getFullURI(), AFF4Datatype.sha256);

	}

	
	public static synchronized TypeMapper getInstance() {
		if (instance == null) {
			instance = new TypeMapper();
		}
		
		return instance;
	}
	
	public DataType map(String uri) {
		return map.get(uri);
	}
}
