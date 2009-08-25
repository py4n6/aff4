package aff4.commonobjects;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.text.ParseException;

import aff4.container.Container;
import aff4.container.WritableStore;
import aff4.infomodel.serialization.PropertiesWriter;
import aff4.storage.zip.StreamWriter;
import aff4.storage.zip.WritableZipVolume;

public class LinkWriter extends ReadWriteInstance {
	StreamWriter target = null;
	String linkName = null;
	public LinkWriter(WritableStore v, StreamWriter target, String name) {
		super(v);
		this.target = target;
		this.linkName = name;

	}

	public void close() throws FileNotFoundException, IOException, ParseException {
		String URN = getURN();
		container.add(URN + "/properties", URN, "aff4:size", Long.toString(target.getSize()));
		container.add(URN + "/properties", URN, "aff4:type", "link");
		container.add(URN + "/properties", URN, "aff4:target", target.getURN());
		container.add(URN + "/properties", URN, "aff4:stored", container.getURN());
		container.add(container.getURN(), container.getURN(), "aff4:contains", URN + "/properties");
		
		String name =  linkName	+ "/properties";
		
		PropertiesWriter writer = new PropertiesWriter(URN);
		String res = writer
				.write(container.query(null, URN, null, null));
		OutputStream f = container.createOutputStream(name, true, res.length());
		f.write(res.getBytes());
		f.close();
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