package aff4.infomodel.test;

import java.util.List;

import aff4.infomodel.Quad;
import aff4.infomodel.QuadStore;

import junit.framework.TestCase;

public class TestQuadStore extends TestCase {
	public void testEmptyQuery() {
		QuadStore q = new QuadStore();
		//q.add("a", "b", "c", "d");
		List<Quad> res = q.query(null, null, null, null);
		assertEquals(0, res.size());
	}
	
	public void testOneEntry() {
		QuadStore q = new QuadStore();
		q.add("a", "b", "c", "d");
		List<Quad> res = q.query(null, null, null, null);
		assertEquals(1, res.size());
		res = q.query("b", null, null, null);
		assertEquals(0, res.size());
		res = q.query(null, "b", null, null);
		assertEquals(1, res.size());
		res = q.query(null, null, "b", null);
		assertEquals(0, res.size());
		res = q.query(null, null, null, "b");
		assertEquals(0, res.size());
		res = q.query("a", null, null, null);
		assertEquals(1, res.size());
		res = q.query(null, "a", null, null);
		assertEquals(0, res.size());
		res = q.query(null, null, "a", null);
		assertEquals(0, res.size());
		res = q.query(null, null, null, "a");
		assertEquals(0, res.size());
		res = q.query("c", null, null, null);
		assertEquals(0, res.size());
		res = q.query(null, "c", null, null);
		assertEquals(0, res.size());
		res = q.query(null, null, "c", null);
		assertEquals(1, res.size());
		res = q.query(null, null, null, "c");
		assertEquals(0, res.size());
		res = q.query("d", null, null, null);
		assertEquals(0, res.size());
		res = q.query(null, "d", null, null);
		assertEquals(0, res.size());
		res = q.query(null, null, "d", null);
		assertEquals(0, res.size());
		res = q.query(null, null, null, "d");
		assertEquals(1, res.size());
		res = q.query("a", "b", null, null);
		assertEquals(1, res.size());
		res = q.query(null, "b", "c", null);
		assertEquals(1, res.size());
		res = q.query(null, null, "c", "d");
		assertEquals(1, res.size());
	}
	
	public void testTwoEntry() {
		QuadStore q = new QuadStore();
		q.add("a", "b", "c", "d");
		q.add("a", "b", "c", "f");
		List<Quad> res = q.query(null, null, null, null);
		assertEquals(2, res.size());
		res = q.query("b", null, null, null);
		assertEquals(0, res.size());
		res = q.query(null, "b", null, null);
		assertEquals(2, res.size());
		res = q.query(null, null, "b", null);
		assertEquals(0, res.size());
		res = q.query(null, null, null, "b");
		assertEquals(0, res.size());
		res = q.query("a", null, null, null);
		assertEquals(2, res.size());
		res = q.query(null, "a", null, null);
		assertEquals(0, res.size());
		res = q.query(null, null, "a", null);
		assertEquals(0, res.size());
		res = q.query(null, null, null, "a");
		assertEquals(0, res.size());
		res = q.query("c", null, null, null);
		assertEquals(0, res.size());
		res = q.query(null, "c", null, null);
		assertEquals(0, res.size());
		res = q.query(null, null, "c", null);
		assertEquals(2, res.size());
		res = q.query(null, null, null, "c");
		assertEquals(0, res.size());
		res = q.query("d", null, null, null);
		assertEquals(0, res.size());
		res = q.query(null, "d", null, null);
		assertEquals(0, res.size());
		res = q.query(null, null, "d", null);
		assertEquals(0, res.size());
		res = q.query(null, null, null, "d");
		assertEquals(1, res.size());
		res = q.query("a", "b", null, null);
		assertEquals(2, res.size());
		res = q.query(null, "b", "c", null);
		assertEquals(2, res.size());
		res = q.query(null, null, "c", "d");
		assertEquals(1, res.size());
	}
	
	public void testResult() {
		QuadStore q = new QuadStore();
		q.add("a", "b", "c", "d");
		q.add("a", "b", "c", "f");
		List<Quad> res = q.query(null, null, null, "f");
		assertEquals(1, res.size());
		Quad r = res.get(0);
		assertEquals("a", r.getGraph());
		assertEquals("b", r.getSubject());
		assertEquals("c", r.getPredicate());
		assertEquals("f", r.getObject());
	}
}
