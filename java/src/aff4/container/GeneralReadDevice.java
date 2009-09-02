package aff4.container;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.text.ParseException;
import java.util.ArrayList;

import aff4.datamodel.MapReader;
import aff4.datamodel.Reader;
import aff4.infomodel.AFFObject;
import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.Resource;
import aff4.infomodel.lexicon.AFF4;
import aff4.storage.zip.ReadOnlyZipVolume;
import aff4.storage.zip.StreamReader;

public class GeneralReadDevice extends AFFObject implements Reader{

	Reader device = null;
	ReadOnlyZipVolume volume = null;
	
	public GeneralReadDevice(ReadOnlyZipVolume vol, Resource urn) throws IOException, ParseException {
		volume = vol;
		
		QuadList res = volume.query(Node.ANY, urn, AFF4.type, Node.ANY);
		ArrayList<Resource> typeList = new ArrayList<Resource>();
		for (Quad q : res) {
			typeList.add((Resource)q.getObject());
		}
		
		if (typeList.contains("image")){
			device =  new StreamReader(volume, urn);
		} else if (typeList.contains("map")){
			device = new MapReader(volume,urn);
		}
	}



	public void position(long s) throws IOException{
		device.position(s);
		
	}
	
	public void close() throws IOException {
		device.close();
	}

	public long position() {
		return device.position();
	}

	public int read(ByteBuffer buf) throws IOException, ParseException {
		return device.read(buf);
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