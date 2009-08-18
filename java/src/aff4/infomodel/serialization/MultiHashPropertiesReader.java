package aff4.infomodel.serialization;

import java.io.IOException;
import java.io.InputStream;
import java.text.ParseException;

public class MultiHashPropertiesReader extends PropertiesReader {
	MultiHashSet res;
	public MultiHashPropertiesReader(InputStream stream) {
		super(stream);
		res = new MultiHashSet();
	}
	
	public MultiHashSet read() throws IOException, ParseException {
		process();
		return res;
	}
	
	protected void doLine(String line) {
		int equalsCharPos = line.indexOf('=');
		String name = line.substring(0, equalsCharPos);
		String value = line.substring(equalsCharPos+1);
		res.put(name,value);
	}

}

/*
Advanced Forensic Format v.4 http://www.afflib.org/

Copyright (C) 2009  Bradley Schatz <bradley@schatzforensic.com.au>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/