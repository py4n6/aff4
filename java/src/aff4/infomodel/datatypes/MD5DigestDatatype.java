package aff4.infomodel.datatypes;

public class MD5DigestDatatype extends AFF4Datatype {

	public MD5DigestDatatype() {
		super("md5");
	}

	public String toLexicalForm(Object o) {
		return o.toString();
	}

	public Object parse(String lexicalForm) {
		return lexicalForm;
	}
}
