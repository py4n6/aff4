package aff4.infomodel.datatypes;

public class CanonicalSHA256DigestDatatype extends AFF4Datatype {

	public CanonicalSHA256DigestDatatype() {
		super("canonical-sha256");
	}

	public String toLexicalForm(Object o) {
		return o.toString();
	}

	public Object parse(String lexicalForm) {
		return lexicalForm;
	}
}
