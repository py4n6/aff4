package aff4.test;

import aff4.container.test.TestContainer;
import aff4.infomodel.aff4il.parser.test.TestAFF4ILWriter;
import aff4.infomodel.aff4il.parser.test.TestIntegrated;
import aff4.infomodel.aff4il.parser.test.TestParser;
import aff4.infomodel.test.TestCanonical;
import aff4.infomodel.test.TestMapResolver;
import aff4.infomodel.test.TestPropertiesFile;
import aff4.infomodel.test.TestQuadStore;
import aff4.infomodel.test.TestSliceResourceParser;
import aff4.util.test.TestStructConverter;

import junit.framework.Test;
import junit.framework.TestSuite;

public class TestAll extends TestSuite {
	   static public Test suite() {
	        return new TestAll();
	    }

	    private TestAll() {
	        super("aff4");
	        addTest(new TestSuite(TestStructConverter.class));
	        addTest(new TestSuite(TestPropertiesFile.class));
	        addTest(new TestSuite(TestSliceResourceParser.class));
	        addTest(new TestSuite(TestIntegrated.class));
	        addTest(new TestSuite(TestParser.class));
	        addTest(new TestSuite(TestAFF4ILWriter.class));
	        addTest(new TestSuite(TestQuadStore.class));
	        addTest(new TestSuite(TestCanonical.class));
	        addTest(new TestSuite(TestMapResolver.class));
	        addTest(new TestSuite(TestContainer.class));
	    }
}
