package aff4.infomodel.datatypes;

import aff4.infomodel.Node;
import aff4.infomodel.Resource;

public abstract class XSDDataType extends DataType{

	public static final String XSD = "http://www.w3.org/2001/XMLSchema";
	
	public XSDDataType(String dataType) {
		super(dataType);
		// TODO Auto-generated constructor stub
	}


	public Resource getURI() {
		return Node.createURI(XSD + "#" + dataType);
	}
	
}
