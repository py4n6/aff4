package aff4.infomodel.test;

import java.io.IOException;
import java.io.InputStream;
import java.text.ParseException;

import aff4.infomodel.Node;
import aff4.infomodel.QuadStore;
import aff4.infomodel.serialization.MapResolver;
import aff4.infomodel.serialization.Properties2Resolver;

import junit.framework.TestCase;

public class TestMapResolver extends TestCase {
	public void testMap2() {
		try {
			InputStream is = TestMapResolver.class.getResourceAsStream("maptriples");
			Properties2Resolver mr = new Properties2Resolver(is, "urn:aff4:a1715880-9112-409a-bbd3-e6f8f3c93539", "urn:aff4:bcb35060-862b-4060-a802-95770e93f7aa");
			QuadStore qs = new QuadStore();
			qs.add(mr.query(null, null, null, null));
			
			assertEquals(8, qs.query(null, "urn:aff4:bcb35060-862b-4060-a802-95770e93f7aa", "aff4:contains", null).size());
		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			fail();
		}
	}
	public void testMapResolver() {

		try {
			InputStream is = TestMapResolver.class.getResourceAsStream("map");
			MapResolver mr = new MapResolver(Node.createURI("foo"), 92208, is);
			mr.query(null,null, null);
		} catch (IOException e) {
			fail();
		} catch (ParseException e) {
			fail();
		}
		
	}
	

}
