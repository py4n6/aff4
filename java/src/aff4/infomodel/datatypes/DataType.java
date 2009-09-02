package aff4.infomodel.datatypes;

import aff4.infomodel.Resource;

public abstract class DataType {
	
	String dataType;
	
	public static final DataType integer = new XSDInteger();


	public DataType(String dataType) {
		this.dataType = dataType;
		
	}
	public Object parse(String lexicalForm) {
		return null;
	}
	
	public abstract Resource getURI();
	
	public abstract String toLexicalForm(Object o); 
}
