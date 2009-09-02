package aff4.infomodel;

import aff4.infomodel.datatypes.DataType;

public class Literal extends Node {
	protected DataType dataType;
	protected Object value;
	
	
	public Literal(DataType type, Object val) {
		value = val;
		dataType = type;
	}
	public Literal(Object val) {
		value = val;
	}
	
	public DataType getDatatype() {
		if (dataType == null)
			return null;
		return dataType;
	}
	
	public Resource getDatatypeURI() {
		if (dataType == null)
			return null;
		return dataType.getURI();
	}
	
	public String getLexicalForm() {
		if (dataType == null) {
			return value.toString();
		} else {
			return dataType.toLexicalForm(value);
		}
	}
	
	public void setValue(Object value) {
		this.value = value;
	}
	
	public long asLong() {
		return (long) ((Long)value);
	}
	
	public String asString() {
		return ((String)value);
	}
	
	public String getURI() {
		if (dataType == null) {
			return value.toString();
		} else {
			return "\"" + getLexicalForm() + "\"^^" + getDatatypeURI();
		}
	}
	
	public String toString() {
		return getLexicalForm();
	}
}
