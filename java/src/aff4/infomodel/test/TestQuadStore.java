package aff4.infomodel.test;

import java.util.List;

import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadStore;
import aff4.infomodel.Resource;

import junit.framework.TestCase;

public class TestQuadStore extends TestCase {
	public void testEmptyQuery() {
		QuadStore q = new QuadStore();
		//q.add("a", new Resource("b"), new Resource("c"), new Resource("d"));
		List<Quad> res = q.query(Node.ANY, Node.ANY, Node.ANY, Node.ANY);
		assertEquals(0, res.size());
	}
	
	public void testOneEntry() {
		QuadStore q = new QuadStore();
		q.add(new Quad(new Resource("a"), new Resource("b"), new Resource("c"), new Resource("d")));
		List<Quad> res = q.query(Node.ANY, Node.ANY, Node.ANY, Node.ANY);
		assertEquals(1, res.size());
		res = q.query(new Resource("b"), Node.ANY, Node.ANY, Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, new Resource("b"), Node.ANY, Node.ANY);
		assertEquals(1, res.size());
		res = q.query(Node.ANY, Node.ANY, new Resource("b"), Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, Node.ANY, Node.ANY, new Resource("b"));
		assertEquals(0, res.size());
		res = q.query(new Resource("a"), Node.ANY, Node.ANY, Node.ANY);
		assertEquals(1, res.size());
		res = q.query(Node.ANY, new Resource("a"), Node.ANY, Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, Node.ANY, new Resource("a"), Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, Node.ANY, Node.ANY, new Resource("a"));
		assertEquals(0, res.size());
		res = q.query(new Resource("c"), Node.ANY, Node.ANY, Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, new Resource("c"), Node.ANY, Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, Node.ANY, new Resource("c"), Node.ANY);
		assertEquals(1, res.size());
		res = q.query(Node.ANY, Node.ANY, Node.ANY, new Resource("c"));
		assertEquals(0, res.size());
		res = q.query(new Resource("d"), Node.ANY, Node.ANY, Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, new Resource("d"), Node.ANY, Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, Node.ANY, new Resource("d"), Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, Node.ANY, Node.ANY, new Resource("d"));
		assertEquals(1, res.size());
		res = q.query(new Resource("a"), new Resource("b"), Node.ANY, Node.ANY);
		assertEquals(1, res.size());
		res = q.query(Node.ANY, new Resource("b"), new Resource("c"), Node.ANY);
		assertEquals(1, res.size());
		res = q.query(Node.ANY, Node.ANY, new Resource("c"), new Resource("d"));
		assertEquals(1, res.size());
	}
	
	public void testTwoEntry() {
		QuadStore q = new QuadStore();
		q.add(new Quad(new Resource("a"), new Resource("b"), new Resource("c"), new Resource("d")));
		q.add(new Quad(new Resource("a"), new Resource("b"), new Resource("c"), new Resource("f")));
		List<Quad> res = q.query(Node.ANY, Node.ANY, Node.ANY, Node.ANY);
		assertEquals(2, res.size());
		res = q.query(new Resource("b"), Node.ANY, Node.ANY, Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, new Resource("b"), Node.ANY, Node.ANY);
		assertEquals(2, res.size());
		res = q.query(Node.ANY, Node.ANY, new Resource("b"), Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, Node.ANY, Node.ANY, new Resource("b"));
		assertEquals(0, res.size());
		res = q.query(new Resource("a"), Node.ANY, Node.ANY, Node.ANY);
		assertEquals(2, res.size());
		res = q.query(Node.ANY, new Resource("a"), Node.ANY, Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, Node.ANY, new Resource("a"), Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, Node.ANY, Node.ANY, new Resource("a"));
		assertEquals(0, res.size());
		res = q.query(new Resource("c"), Node.ANY, Node.ANY, Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, new Resource("c"), Node.ANY, Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, Node.ANY, new Resource("c"), Node.ANY);
		assertEquals(2, res.size());
		res = q.query(Node.ANY, Node.ANY, Node.ANY, new Resource("c"));
		assertEquals(0, res.size());
		res = q.query(new Resource("d"), Node.ANY, Node.ANY, Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, new Resource("d"), Node.ANY, Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, Node.ANY, new Resource("d"), Node.ANY);
		assertEquals(0, res.size());
		res = q.query(Node.ANY, Node.ANY, Node.ANY, new Resource("d"));
		assertEquals(1, res.size());
		res = q.query(new Resource("a"), new Resource("b"), Node.ANY, Node.ANY);
		assertEquals(2, res.size());
		res = q.query(Node.ANY, new Resource("b"), new Resource("c"), Node.ANY);
		assertEquals(2, res.size());
		res = q.query(Node.ANY, Node.ANY, new Resource("c"), new Resource("d"));
		assertEquals(1, res.size());
	}
	
	public void testResult() {
		QuadStore q = new QuadStore();
		q.add(new Quad(new Resource("a"), new Resource("b"), new Resource("c"), new Resource("d")));
		q.add(new Quad(new Resource("a"), new Resource("b"), new Resource("c"), new Resource("f")));
		List<Quad> res = q.query(Node.ANY, Node.ANY, Node.ANY, new Resource("f"));
		assertEquals(1, res.size());
		Quad r = res.get(0);
		assertEquals(new Resource("a"), r.getGraph());
		assertEquals(new Resource("b"), r.getSubject());
		assertEquals(new Resource("c"), r.getPredicate());
		assertEquals(new Resource("f"), r.getObject());
	}
}
