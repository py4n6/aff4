package aff4.infomodel.resource;

import junit.framework.TestCase;

public class TestSliceResourceParser extends TestCase {
	public void testParser() {
		ResourceParser parser = new ResourceParser();
		SliceResource res = parser.parseSlice("urn:aff4:da0d1948-846f-491d-8183-34ae691e8293[0,92208]");
		assertEquals("urn:aff4:da0d1948-846f-491d-8183-34ae691e8293", res.URN);
		assertEquals(92208, res.length);
		assertEquals(0,res.offset);
		
	}
}
