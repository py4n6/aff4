package aff4.infomodel.test;

import aff4.infomodel.Node;
import aff4.infomodel.Resource;
import aff4.infomodel.SliceResource;
import junit.framework.TestCase;

public class TestSliceResourceParser extends TestCase {
	public void testSliceResource() {

		Resource res = Node.createURI("urn:aff4:da0d1948-846f-491d-8183-34ae691e8293[0:92208]");
		assertTrue(res instanceof SliceResource);
		SliceResource resa = (SliceResource) res;
		assertEquals("urn:aff4:da0d1948-846f-491d-8183-34ae691e8293", resa.getURI());
		assertEquals(92208, resa.getLength());
		assertEquals(0,resa.getOffset());
	}
	
	public void testStandardResource() {
		Resource res = Node.createURI("urn:aff4:da0d1948-846f-491d-8183-34ae691e8293");
		assertEquals("urn:aff4:da0d1948-846f-491d-8183-34ae691e8293", res.getURI());

	}
	
}
