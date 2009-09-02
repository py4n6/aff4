package aff4.infomodel;

import java.util.HashSet;

public class NamedGraphSet extends HashSet<Quad> {

	public boolean containsGraph(Node n) {
		return contains(n);
	}
	
	public void createGraph(Node n) {
		
	}

	public void addQuad(Quad q ) {
		add(q);
	}
}