package aff4.infomodel.resource;

public class Resource {

	public Resource(String urn) {
		URN = urn;
	}
	public String getBaseURN() {
		return URN;
	}

	public void setBaseURN(String urn) {
		URN = urn;
	}

	protected String URN;

}
