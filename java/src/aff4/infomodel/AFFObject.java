package aff4.infomodel;

import java.util.UUID;

public class AFFObject {
	Resource URN = null;
	
	public Resource getURN() {
		if (URN == null) {
			URN = Node.createURI("urn:aff4:" + UUID.randomUUID().toString());
		}
		return URN;
	}
}
