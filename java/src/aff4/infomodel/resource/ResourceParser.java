package aff4.infomodel.resource;

import aff4.infomodel.serialization.Point;

public class ResourceParser {

	public static SliceResource parseSlice(String res) {
		int startBracket = res.indexOf('[');
		int comma = res.indexOf(',',startBracket+1);
		int endBracket = res.indexOf(']',comma+1);
		String urn = res.substring(0,startBracket);
		String offset = res.substring(startBracket+1,comma);
		String length = res.substring(comma+1,endBracket);
		
		long off = Long.parseLong(offset);
		long len = Long.parseLong(length);

		return new SliceResource(urn, off, len);
		
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