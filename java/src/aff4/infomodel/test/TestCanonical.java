package aff4.infomodel.test;

import java.util.ArrayList;

import aff4.infomodel.GraphCanonicalizer;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import junit.framework.TestCase;

public class TestCanonical extends TestCase {
	public void testCanonical() {
		QuadList ql = new QuadList();
		ql.add(new Quad(null, "a", "b", "c"));
		ql.add(new Quad(null, "a", "c", "c"));
		
		GraphCanonicalizer can = new GraphCanonicalizer(ql);
		ArrayList<String> canonical = can.getCanonicalStringsArray();
		assertEquals("a b c", canonical.get(0));
		assertEquals("a c c", canonical.get(1));
	}
	
	public void testCanonicalA() {
		QuadList ql = new QuadList();
		ql.add(new Quad(null, "a", "c", "c"));
		ql.add(new Quad(null, "a", "b", "c"));

		
		GraphCanonicalizer can = new GraphCanonicalizer(ql);
		ArrayList<String> canonical = can.getCanonicalStringsArray();
		assertEquals("a b c", canonical.get(0));
		assertEquals("a c c", canonical.get(1));
	}
}
