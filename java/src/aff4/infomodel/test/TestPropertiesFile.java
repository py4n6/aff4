package aff4.infomodel.test;

import java.io.IOException;
import java.io.InputStream;
import java.text.ParseException;

import junit.framework.TestCase;
import aff4.infomodel.QuadList;
import aff4.infomodel.serialization.MultiHashPropertiesReader;
import aff4.infomodel.serialization.MultiHashSet;
import aff4.infomodel.serialization.Properties2Resolver;

public class TestPropertiesFile extends TestCase {
	
	public void testBadContent() {
		InputStream str = null;
		try {
			str = TestPropertiesFile.class.getResourceAsStream("badproperties");
			Properties2Resolver pr = new Properties2Resolver(str, "A", "B");
			QuadList res = pr.query(null,null,null,null);
			fail();
			
		} catch (ParseException ex) {
			
		} catch (IOException ex) {
			fail();
		} finally {
			try {
				str.close();
			}catch (IOException ex) {
				
			}
		}
	}
	public void testSize() {
		try {
			InputStream str = TestPropertiesFile.class.getResourceAsStream("properties");
			MultiHashPropertiesReader pr = new MultiHashPropertiesReader(str);
			MultiHashSet p = pr.read();
			assertEquals(16, p.size());
			assertEquals(1, p.get("aff4:stored").size());
		} catch (IOException ex) {
			fail();
		} catch (ParseException ex) {
			fail();
		}
	}
	
	public void testProperties2() {
		try {
			InputStream str = TestPropertiesFile.class.getResourceAsStream("properties2");
			Properties2Resolver pr = new Properties2Resolver(str, "A", "B");
			QuadList res = pr.query(null,null,null,null);
			assertEquals(20, res.size());
			str.close();
			
			str = TestPropertiesFile.class.getResourceAsStream("properties2");
			pr = new Properties2Resolver(str, "A", "B");
			res = pr.query("A",null,null,null);
			assertEquals(20, res.size());
			str.close();
			
			str = TestPropertiesFile.class.getResourceAsStream("properties2");
			pr = new Properties2Resolver(str, "A", "B");
			res = pr.query("B",null,null,null);
			assertEquals(0, res.size());
			
			str = TestPropertiesFile.class.getResourceAsStream("properties2");
			pr = new Properties2Resolver(str, "A", "B");
			res = pr.query(null,"A",null,null);
			assertEquals(0, res.size());			
			
			str = TestPropertiesFile.class.getResourceAsStream("properties2");
			pr = new Properties2Resolver(str, "A", "B");
			res = pr.query(null,"B",null,null);
			assertEquals(18, res.size());
			
			str = TestPropertiesFile.class.getResourceAsStream("properties2");
			pr = new Properties2Resolver(str, "A", "B");
			res = pr.query(null,null,"C",null);
			assertEquals(2, res.size());
			
			str = TestPropertiesFile.class.getResourceAsStream("properties2");
			pr = new Properties2Resolver(str, "A", "B");
			res = pr.query(null,"B","C",null);
			assertEquals(2, res.size());			
	
			str = TestPropertiesFile.class.getResourceAsStream("properties2");
			pr = new Properties2Resolver(str, "A", "B");
			res = pr.query(null,null,"C","D");
			assertEquals(1, res.size());	
			
			str = TestPropertiesFile.class.getResourceAsStream("properties2");
			pr = new Properties2Resolver(str, "A", "B");
			res = pr.query(null,null,null,"D");
			assertEquals(1, res.size());

			str = TestPropertiesFile.class.getResourceAsStream("properties2");
			pr = new Properties2Resolver(str, "A", "B");
			res = pr.query("A","B","C","D");
			assertEquals(1, res.size());
			
			str = TestPropertiesFile.class.getResourceAsStream("properties2");
			pr = new Properties2Resolver(str, "A", "B");
			res = pr.query(null,null,"E",null);
			assertEquals(2, res.size());
			
			str = TestPropertiesFile.class.getResourceAsStream("properties2");
			pr = new Properties2Resolver(str, "A", "B");
			res = pr.query(null,null,"E","F");
			assertEquals(1, res.size());
			
		} catch (IOException ex) {
			fail();
		} catch (ParseException ex) {
			fail();
		}
	}
}
