package aff4.infomodel.aff4il.parser.test;

import java.io.IOException;
import java.io.StringReader;
import java.io.StringWriter;

import aff4.infomodel.Literal;
import aff4.infomodel.NamedGraphSet;
import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QuadStore;
import aff4.infomodel.Resource;
import aff4.infomodel.aff4il.NamedGraphSetPopulator;
import aff4.infomodel.aff4il.AFF4ILWriter;
import aff4.infomodel.aff4il.parser.AFF4ILParser;
import aff4.infomodel.datatypes.XSDInteger;
import antlr.RecognitionException;
import antlr.TokenStreamException;
import junit.framework.TestCase;

public class TestIntegrated extends TestCase {
	public void testSimple() {
		try {
			NamedGraphSet quads = new NamedGraphSet();
			quads.add(new Quad(new Resource("http://foo.net/G1"), new Resource("http:/foor.net/S1"), new Resource("http://foo.net/name"), new Literal(new XSDInteger(), "123456")));
			StringWriter sw = new StringWriter();
			AFF4ILWriter tw = new AFF4ILWriter();
			tw.addNamespace("foo", "http://foo.net/");
			tw.write(new QuadStore(quads), sw, "http://foo.net/");
			
			StringReader r = new StringReader(sw.toString());
			NamedGraphSet ngs = new NamedGraphSet();
			NamedGraphSetPopulator ngsp = new NamedGraphSetPopulator(ngs, "foo", "foo");
			AFF4ILParser parser = new AFF4ILParser(r, ngsp);
			

			parser.parse();
			QuadStore store = new QuadStore();
			store.add(ngs);
			QuadList res = store.query(Node.createURI("http://foo.net/G1"), null, null, null);
			assertEquals(1, res.size());
			assertEquals("http://foo.net/G1", res.get(0).getGraph().getURI());
			assertEquals("http:/foor.net/S1", res.get(0).getSubject().getURI());
			assertEquals("http://foo.net/name", res.get(0).getPredicate().getURI());
			assertEquals(123456, ((Literal)res.get(0).getObject()).asLong());

			
			
			
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (RecognitionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (TokenStreamException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
			

	}
}
