package aff4.util.test;

import aff4.util.StructConverter;
import junit.framework.TestCase;

public class TestStructConverter extends TestCase {
	public void testU32Int() {
		byte[] b = { (byte)0xff, (byte)0xff, (byte)0xff, (byte)0xff};
		long res = StructConverter.bytesIntoUInt(b, 0);
		assertEquals(0xffffffffL, res);
		
		b = StructConverter.UInt2bytes(0xffffffffL);
		assertEquals((byte)0xff, b[0]);
		assertEquals((byte)0xff, b[1]);
		assertEquals((byte)0xff, b[2]);
		assertEquals((byte)0xff, b[3]);
	}
	
	
}
