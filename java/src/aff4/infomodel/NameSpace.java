package aff4.infomodel;

import java.util.HashMap;
import java.util.Map;

public class NameSpace {

	static Map<String,String> nsMap = new HashMap<String, String>();
	
	public static Map<String,String> getNsPrefixMap() {
		return nsMap;
	}
}
