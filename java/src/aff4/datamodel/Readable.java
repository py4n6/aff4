package aff4.datamodel;

import java.io.IOException;
import java.text.ParseException;

import aff4.infomodel.Queryable;
import aff4.infomodel.Resource;

public interface Readable extends Queryable {
	public Reader open(Resource segment) throws IOException, ParseException;
}
