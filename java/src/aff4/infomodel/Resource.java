package aff4.infomodel;

import java.util.List;

public class Resource extends Node implements Comparable<Resource>, Cloneable{
	String localName = null;
	String nameSpace = null;
	String URI;
	
	public Resource(String uri) {
		this.URI = uri;
	}
	
	public Resource(String ns, String localPart) {
		this.nameSpace = ns;
		this.localName = localPart;
	}
	
	public void setNameSpace(String ns) {
		nameSpace = ns;
	}
	public void setLocalName(String name) {
		localName = name;
	}
	
	public String getLocalName() {
		return localName;
	}
	
	
	public String getNameSpace() {
		return nameSpace;
	}

	public String getURI() {
		if (nameSpace != null && localName != null) {
			return nameSpace + ":" + localName;
		} else {
			return URI;
		}
	}
	
	public String getBaseURI() {
		return URI;
	}
	public String getFullURI() {
		return URI;
	}
	
	public boolean equals(Object o) {
		if (! (o instanceof Resource)) {
			return false;
		}
		Resource r = (Resource) o;
		if (URI.equals(r.URI)) {
			return true;
		}
		return false;
	}

	public int compareTo(Resource arg0) {
		return URI.compareTo(arg0.URI);
	}
	
	public String toString() {
		return getURI();
	}
	
	public Resource clone() {
		if (URI != null) {
			return new Resource(URI);
		}
		if (localName != null && nameSpace != null) {
			return new Resource(nameSpace, localName);
		}
		
		return null;
	}
}
