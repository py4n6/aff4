package aff4.infomodel.datatypes;

public class SHA256DigestDatatype extends AFF4Datatype {

	public SHA256DigestDatatype() {
		super("sha256");
	}

	public String toLexicalForm(Object o) {
		return o.toString();
	}

	public Object parse(String lexicalForm) {
		return lexicalForm;
	}
}
