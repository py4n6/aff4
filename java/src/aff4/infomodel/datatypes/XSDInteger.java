package aff4.infomodel.datatypes;

import aff4.infomodel.Literal;

public class XSDInteger extends XSDDataType {

	public XSDInteger() {
		super("integer");
	}
	
	public Object parse(String lexicalForm) {
		return Long.parseLong(lexicalForm);
	}

	public String toLexicalForm(Object o) {
		return o.toString();
	}
}
