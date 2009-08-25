package aff4.datamodel;

import java.io.IOException;
import java.text.ParseException;

import aff4.infomodel.Queryable;

public interface Readable extends Queryable {
	public Reader open(String segment) throws IOException, ParseException;
}
