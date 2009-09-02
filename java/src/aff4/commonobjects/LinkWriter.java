package aff4.commonobjects;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.text.ParseException;

import aff4.container.Container;
import aff4.container.WritableStore;
import aff4.infomodel.Node;
import aff4.infomodel.Resource;
import aff4.infomodel.datatypes.DataType;
import aff4.infomodel.lexicon.AFF4;
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
		Resource URN = getURN();
		container.add(container.getGraph(), URN, AFF4.size, Node.createLiteral(Long.toString(target.getSize()), null, DataType.integer));
		container.add(container.getGraph(), URN, AFF4.type, Node.createLiteral("link", null, null));
		container.add(container.getGraph(), URN, AFF4.target, target.getURN());
		container.add(container.getGraph(), URN, AFF4.stored, container.getURN());
		container.add(container.getGraph(), container.getURN(), AFF4.contains, Node.createURI(URN.getURI() + "/properties"));
		
		/*
		String name =  linkName	+ "/properties";
		
		PropertiesWriter writer = new PropertiesWriter(URN);
		String res = writer
				.write(container.query(null, URN, null, null));
		OutputStream f = container.createOutputStream(name, true, res.length());
		f.write(res.getBytes());
		f.close();
		*/
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