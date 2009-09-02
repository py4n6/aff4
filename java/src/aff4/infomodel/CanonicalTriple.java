package aff4.infomodel;


public class CanonicalTriple extends Object implements Comparable<CanonicalTriple> {
	public Node subject;
	public Node predicate;
	public Node object;

	public CanonicalTriple(String s, String p, String o) {
		this.subject = Node.createURI(s);
		this.predicate = Node.createURI(p);
		this.object = Node.createLiteral(o, null, null);
	}

	public CanonicalTriple(Node s, Node p, Node o) {
		this.subject = s;
		this.predicate = p;
		this.object = o;
	}
	
	public CanonicalTriple(Quad q) {
		this.subject = q.getSubject();
		this.predicate = q.getPredicate();
		this.object = q.getObject();
	}

	public CanonicalTriple(CanonicalTriple st) {
		this.object = st.object;
		this.predicate = st.predicate;
		this.subject = st.subject;

	}

	public int compareTo(CanonicalTriple arg) throws ClassCastException {
		String current = subject.getURI() + predicate.getURI()
				+ object.getURI();

		String ext = arg.subject.getURI() + arg.predicate.getURI()
				+ arg.object.getURI();
		return current.compareTo(ext);
	}

	public String createTripleString() {
		return subject.getURI() + " " + predicate.getURI() + " "
				+ object.getURI();
	}
	
	public String toString() {
		return createTripleString();
	}
}

