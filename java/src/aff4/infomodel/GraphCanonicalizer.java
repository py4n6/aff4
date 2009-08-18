package aff4.infomodel;

import java.util.ArrayList;
import java.util.Collections;


public class GraphCanonicalizer {

	private ArrayList<String> canonical_string;

	public ArrayList<String> getCanonicalStringsArray() {
		return canonical_string;
	}

	public GraphCanonicalizer(QuadList quads) {
		canonical_string = new ArrayList<String>();
		ArrayList<CanonicalTriple> pre_canonic = toCanonicalTriples(quads); // 1)

		for (CanonicalTriple t : pre_canonic) {
			canonical_string.add(t.createTripleString());
		}
	}


	private ArrayList<CanonicalTriple> toCanonicalTriples(QuadList a) {
		// filter out any signature triples
		ArrayList<CanonicalTriple> am = new ArrayList<CanonicalTriple>();
		for (Quad tmp : a) {
			if (!tmp.getPredicate().equals("aff4:signature")) {
				am.add(new CanonicalTriple(tmp));
			}
		}
		
		Collections.sort(am);
		return am;
	}


	public String getCanonicalString() {
		StringBuffer canonicalData = new StringBuffer();
		for (String line: getCanonicalStringsArray()) {
			canonicalData.append(line);
			canonicalData.append(",");
		}
		return canonicalData.toString();
	}
}

