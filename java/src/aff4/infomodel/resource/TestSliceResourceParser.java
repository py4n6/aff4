package aff4.infomodel.resource;

import junit.framework.TestCase;

public class TestSliceResourceParser extends TestCase {
	public void testSliceResource() {

		Resource res = ResourceParser.parse("urn:aff4:da0d1948-846f-491d-8183-34ae691e8293[0:92208]");
		assertTrue(res instanceof SliceResource);
		SliceResource resa = (SliceResource) res;
		assertEquals("urn:aff4:da0d1948-846f-491d-8183-34ae691e8293", resa.URN);
		assertEquals(92208, resa.length);
		assertEquals(0,resa.offset);
	}
	
	public void testStandardResource() {
		Resource res = ResourceParser.parse("urn:aff4:da0d1948-846f-491d-8183-34ae691e8293");
		assertEquals("urn:aff4:da0d1948-846f-491d-8183-34ae691e8293", res.URN);

	}
	
}
