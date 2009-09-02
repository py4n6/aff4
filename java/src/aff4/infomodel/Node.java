package aff4.infomodel;

import aff4.infomodel.datatypes.DataType;

public abstract class Node {
	public static Node ANY;
	
	public static Resource createURI(String uri) {
		return parse(uri);
	}
	
	public abstract String getURI();
	
	public static Literal createLiteral(String lexicalForm, String language, DataType type) {
		if (type == null) {
			return new Literal(lexicalForm);
		} else {
			Object res = type.parse(lexicalForm);
			return new Literal(type, res);
		}

	}
	
	protected static Resource parse(String res) {
		int startBracket = res.indexOf('[');
		int comma = res.indexOf(':',startBracket+1);
		int endBracket = res.indexOf(']',comma+1);
		
		if (startBracket > 0 && endBracket > startBracket) {
			return parseSlice(res);
		} else {
			return new Resource(res);
		}
	}
	
	protected static SliceResource parseSlice(String res) {
		int startBracket = res.indexOf('[');
		int comma = res.indexOf(':',startBracket+1);
		int endBracket = res.indexOf(']',comma+1);
		String urn = res.substring(0,startBracket);
		String offset = res.substring(startBracket+1,comma);
		String length = res.substring(comma+1,endBracket);
		
		long off = Long.parseLong(offset);
		long len = Long.parseLong(length);

		return new SliceResource(res, off, len);
		
	}
}
