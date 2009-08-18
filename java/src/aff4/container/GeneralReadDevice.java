package aff4.container;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.text.ParseException;
import java.util.ArrayList;

import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.storage.AFFObject;
import aff4.storage.MapReader;
import aff4.storage.StreamReader;
import aff4.storage.Reader;
import aff4.storage.ReadOnlyZipVolume;

public class GeneralReadDevice extends AFFObject implements Reader{

	Reader device = null;
	ReadOnlyZipVolume volume = null;
	
	public GeneralReadDevice(ReadOnlyZipVolume vol, String urn) throws IOException, ParseException {
		volume = vol;
		String URN = getURN();
		
		QuadList res = volume.query(null, urn, "aff4:type", null);
		ArrayList<String> typeList = new ArrayList<String>();
		for (Quad q : res) {
			typeList.add(q.getObject());
		}
		
		if (typeList.contains("image")){
			device =  new StreamReader(volume, urn);
		} else if (typeList.contains("map")){
			device = new MapReader(volume,urn);
		}
	}

	public ByteBuffer read(long length) throws IOException, ParseException {
		// TODO Auto-generated method stub
		return device.read(length);
	}


	public void seek(long s) {
		device.seek(s);
		
	}
	
	public void close() {
		device.close();
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