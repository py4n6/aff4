package aff4.infomodel.aff4il.parser.test;

import java.io.IOException;
import java.io.StringWriter;
import java.util.HashSet;

import aff4.infomodel.Literal;
import aff4.infomodel.NamedGraphSet;
import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QuadStore;
import aff4.infomodel.Resource;
import aff4.infomodel.aff4il.AFF4ILWriter;
import aff4.infomodel.datatypes.XSDInteger;
import junit.framework.TestCase;

public class TestAFF4ILWriter extends TestCase {
	public void testSimple() {
		NamedGraphSet quads = new NamedGraphSet();
		quads.add(new Quad("http://foo.net/G1", "http:/foor.net/S1", "http://foo.net/name", "bar"));
		quads.add(new Quad("http://foo.net/G1", "http:/foor.net/S1", "http://purl.org/dc/elements/1.1/name", "boo"));
		StringWriter sw = new StringWriter();
		AFF4ILWriter tw = new AFF4ILWriter();
		tw.addNamespace("foo", "http://foo.net/");
		try {
			tw.write(new QuadStore(quads), sw, "http://foo.net/");
			System.out.println(sw.toString());
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
	}
	
	public void testSimple1() {
		NamedGraphSet quads = new NamedGraphSet();
		quads.add(new Quad(new Resource("http://foo.net/G1"), new Resource("http:/foor.net/S1"), new Resource("http://foo.net/name"), new Literal(new XSDInteger(), "123456")));
		StringWriter sw = new StringWriter();
		AFF4ILWriter tw = new AFF4ILWriter();
		tw.addNamespace("foo", "http://foo.net/");
		try {
			tw.write(new QuadStore(quads), sw, "http://foo.net/");
			System.out.println(sw.toString());
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
	}
	
	public void testSliceURNs() {
		NamedGraphSet quads = new NamedGraphSet();
		quads.add(new Quad(new Resource("http://foo.net/G1"), Node.createURI("urn:aff4:alkdjf-234-3445#[100:100]"), new Resource("http://foo.net/name"), new Literal(new XSDInteger(), "123456")));
		StringWriter sw = new StringWriter();
		AFF4ILWriter tw = new AFF4ILWriter();
		tw.addNamespace("foo", "http://foo.net/");
		try {
			tw.write(new QuadStore(quads), sw, "http://foo.net/");
			System.out.println(sw.toString());
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
	}
}
