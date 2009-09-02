package aff4.infomodel.datatypes;

public class CanonicalSHA256RSASignatureDatatype extends AFF4Datatype {

	public CanonicalSHA256RSASignatureDatatype() {
		super("canonical-sha256-rsa");
	}

	public String toLexicalForm(Object o) {
		return o.toString();
	}

	public Object parse(String lexicalForm) {
		return lexicalForm;
	}
}
