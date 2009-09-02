package aff4.infomodel.aff4il.parser.test;

import java.io.StringReader;

import aff4.infomodel.Literal;
import aff4.infomodel.NamedGraphSet;
import aff4.infomodel.Node;
import aff4.infomodel.QuadList;
import aff4.infomodel.QuadStore;
import aff4.infomodel.Resource;
import aff4.infomodel.aff4il.NamedGraphSetPopulator;
import aff4.infomodel.aff4il.parser.AFF4ILParser;
import aff4.infomodel.datatypes.AFF4Datatype;
import antlr.RecognitionException;
import antlr.TokenStreamException;
import junit.framework.TestCase;

public class TestParser extends TestCase {

	
	
	public void testType() {
		StringReader r = new StringReader(new String("@prefix aff4: <http://aff.org/2009/aff4#> .\r\n <foo> { <bar#[786432:65536]>		aff4:hash		\"de2f256064a0af797747c2b97505dc0b9f3df0de4f489eac731c23ae9ca9cc31\"^^aff4:sha256 . }"));

		NamedGraphSet ngs = new NamedGraphSet();
		NamedGraphSetPopulator ngsp = new NamedGraphSetPopulator(ngs, "foo", "foo");
		AFF4ILParser parser = new AFF4ILParser(r, ngsp);
		
		try {
			parser.parse();
			QuadStore store = new QuadStore();
			store.add(ngs);
			QuadList res = store.query(Node.createURI("foo"), null, null, null);
			assertEquals(1, res.size());
			assertEquals("foo", res.get(0).getGraph().getURI());
			assertEquals("bar#[786432:65536]", res.get(0).getSubject().getURI());
			assertEquals("http://aff.org/2009/aff4#hash", res.get(0).getPredicate().getURI());
			assertEquals("de2f256064a0af797747c2b97505dc0b9f3df0de4f489eac731c23ae9ca9cc31", ((Literal)res.get(0).getObject()).asString());
			assertEquals(AFF4Datatype.sha256, ((Literal)res.get(0).getObject()).getDatatype());
		} catch (RecognitionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (TokenStreamException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	public void testSliceURN() {
		StringReader r = new StringReader(new String("@prefix u1: <uri:aff4:093023984-234234-2432343-23423423#> . \r\n @prefix : <http://www.aff4.org/2009#> .\r\n <uri:aff4:093023984-234234-2432343-23423423> { u1:[100:100] :value \"100.00.odx\" . }"));

		NamedGraphSet ngs = new NamedGraphSet();
		NamedGraphSetPopulator ngsp = new NamedGraphSetPopulator(ngs, "foo", "foo");
		AFF4ILParser parser = new AFF4ILParser(r, ngsp);
		
		try {
			parser.parse();
			QuadStore store = new QuadStore();
			store.add(ngs);
			QuadList res = store.query(Node.createURI("uri:aff4:093023984-234234-2432343-23423423"), null, null, null);
			assertEquals(1, res.size());
			assertEquals("uri:aff4:093023984-234234-2432343-23423423", res.get(0).getGraph().getURI());
			assertEquals("uri:aff4:093023984-234234-2432343-23423423#[100:100]", res.get(0).getSubject().getURI());
			assertEquals("http://www.aff4.org/2009#value", res.get(0).getPredicate().getURI());
			assertEquals("100.00.odx", ((Literal)res.get(0).getObject()).asString());
		} catch (RecognitionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (TokenStreamException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public void testDot() {
		StringReader r = new StringReader(new String("<foo> { <bar> <foo> <10000.odx> . }"));
		NamedGraphSet ngs = new NamedGraphSet();
		NamedGraphSetPopulator ngsp = new NamedGraphSetPopulator(ngs, "foo", "foo");
		AFF4ILParser parser = new AFF4ILParser(r, ngsp);
		
		try {
			parser.parse();
			QuadStore store = new QuadStore();
			store.add(ngs);
			QuadList res = store.query(Node.createURI("foo"), null, null, null);
			assertEquals(1, res.size());
			assertEquals("foo", res.get(0).getGraph().getURI());
			assertEquals("bar", res.get(0).getSubject().getURI());
			assertEquals("foo", res.get(0).getPredicate().getURI());
			assertEquals("10000.odx", ((Resource)res.get(0).getObject()).getURI());
		} catch (RecognitionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (TokenStreamException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public void testDot1() {
		StringReader r = new StringReader(new String("@prefix u1: <uri:aff4:093023984-234234-2432343-23423423/> . \r\n @prefix : <http://www.aff4.org/2009#> .\r\n <uri:aff4:093023984-234234-2432343-23423423> { <uri:aff4:093023984-234234-2432343-23423423> :value u1:100.00.odx . }"));
		NamedGraphSet ngs = new NamedGraphSet();
		NamedGraphSetPopulator ngsp = new NamedGraphSetPopulator(ngs, "foo", "foo");
		AFF4ILParser parser = new AFF4ILParser(r, ngsp);
		
		try {
			parser.parse();
			QuadStore store = new QuadStore();
			store.add(ngs);
			QuadList res = store.query(Node.createURI("uri:aff4:093023984-234234-2432343-23423423"), null, null, null);
			assertEquals(1, res.size());
			assertEquals("uri:aff4:093023984-234234-2432343-23423423", res.get(0).getGraph().getURI());
			assertEquals("uri:aff4:093023984-234234-2432343-23423423", res.get(0).getSubject().getURI());
			assertEquals("http://www.aff4.org/2009#value", res.get(0).getPredicate().getURI());
			assertEquals("uri:aff4:093023984-234234-2432343-23423423/100.00.odx", ((Resource)res.get(0).getObject()).getURI());
		} catch (RecognitionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (TokenStreamException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public void testInt() {
		StringReader r = new StringReader(new String("@prefix u1: <uri:aff4:093023984-234234-2432343-23423423> . \r\n @prefix : <http://www.aff4.org/2009#> .\r\n u1: { u1: :value 119987 . }"));
		NamedGraphSet ngs = new NamedGraphSet();
		NamedGraphSetPopulator ngsp = new NamedGraphSetPopulator(ngs, "foo", "foo");
		AFF4ILParser parser = new AFF4ILParser(r, ngsp);
		
		try {
			parser.parse();
			QuadStore store = new QuadStore();
			store.add(ngs);
			QuadList res = store.query(Node.createURI("uri:aff4:093023984-234234-2432343-23423423"), null, null, null);
			assertEquals(1, res.size());
			assertEquals("uri:aff4:093023984-234234-2432343-23423423", res.get(0).getGraph().getURI());
			assertEquals("uri:aff4:093023984-234234-2432343-23423423", res.get(0).getSubject().getURI());
			assertEquals("http://www.aff4.org/2009#value", res.get(0).getPredicate().getURI());
			assertEquals(119987, ((Literal)res.get(0).getObject()).asLong());
		} catch (RecognitionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (TokenStreamException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public void testParser() {
		StringReader r = new StringReader(new String("@prefix : <http://www.example.org/exampleDocument#> . \r\n:G1 { :Monica :name \"Monica Murphy\" . }"));
		NamedGraphSet ngs = new NamedGraphSet();
		NamedGraphSetPopulator ngsp = new NamedGraphSetPopulator(ngs, "foo", "foo");
		AFF4ILParser parser = new AFF4ILParser(r, ngsp);
		
		try {
			parser.parse();
			QuadStore store = new QuadStore();
			store.add(ngs);
			QuadList res = store.query(Node.createURI("http://www.example.org/exampleDocument#G1"), null, null, null);
			assertEquals(1, res.size());
			assertEquals("http://www.example.org/exampleDocument#G1", res.get(0).getGraph().getURI());
			assertEquals("http://www.example.org/exampleDocument#Monica", res.get(0).getSubject().getURI());
			assertEquals("http://www.example.org/exampleDocument#name", res.get(0).getPredicate().getURI());
			assertEquals("Monica Murphy", ((Literal)res.get(0).getObject()).asString());
		} catch (RecognitionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (TokenStreamException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public void testAlias() {
		StringReader r = new StringReader(new String("@prefix u1: <uri:aff4:093023984-234234-2432343-23423423> . \r\n @prefix : <http://www.aff4.org/2009#> .\r\n u1: { u1: :name \"Monica Murphy\" . }"));
		NamedGraphSet ngs = new NamedGraphSet();
		NamedGraphSetPopulator ngsp = new NamedGraphSetPopulator(ngs, "foo", "foo");
		AFF4ILParser parser = new AFF4ILParser(r, ngsp);
		
		try {
			parser.parse();
			QuadStore store = new QuadStore();
			store.add(ngs);
			QuadList res = store.query(Node.createURI("uri:aff4:093023984-234234-2432343-23423423"), null, null, null);
			assertEquals(1, res.size());
			assertEquals("uri:aff4:093023984-234234-2432343-23423423", res.get(0).getGraph().getURI());
			assertEquals("uri:aff4:093023984-234234-2432343-23423423", res.get(0).getSubject().getURI());
			assertEquals("http://www.aff4.org/2009#name", res.get(0).getPredicate().getURI());
			assertEquals("Monica Murphy", ((Literal)res.get(0).getObject()).asString());
		} catch (RecognitionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (TokenStreamException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	

}
