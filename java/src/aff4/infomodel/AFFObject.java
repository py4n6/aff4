package aff4.infomodel;

import java.util.UUID;

public class AFFObject {
	String URN = null;
	
	public String getURN() {
		if (URN == null) {
			URN = "urn:aff4:" + UUID.randomUUID().toString();
		}
		return URN;
	}
}
