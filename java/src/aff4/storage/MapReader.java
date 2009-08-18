package aff4.storage;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.text.ParseException;
import java.util.ArrayList;

import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.resource.ResourceParser;
import aff4.infomodel.resource.SliceResource;
import aff4.infomodel.serialization.Point;

public class MapReader implements Reader{
	ArrayList<Point> points = null;
	String URN = null;
	long size;
	long readptr;
	ReadOnlyZipVolume volume = null;
	
	public MapReader(ReadOnlyZipVolume v, String u) throws IOException, ParseException {
		URN = u;
		volume = v;
		points = new ArrayList<Point>();
		QuadList res = v.query(null, URN, "aff4:contains", null);
		for (Quad q : res) {
			SliceResource part = ResourceParser.parseSlice(q.getObject());
			String targetStr = v.queryValue(null, q.getObject(), "aff4:sameAs");
			Point p = null;
			if (targetStr.equals("urn:aff4:UnknownData")) {
				p = new Point(part.getOffset(), part.getLength(), -1,"urn:aff4:UnknownData");
			} else {
				SliceResource target = ResourceParser.parseSlice(targetStr);
				p = new Point(part.getOffset(), part.getLength(), target.getOffset(), target.getURN());
			}
			points.add(p);
		}
		
		size = Long.parseLong(v.queryValue(null, URN, "aff4:size"));
		readptr = 0;
	}
	
    public ByteBuffer partial_read(long length) throws IOException, ParseException {
    	int i;
    	Point p = null;
    	for (i = 0 ; i < points.size() ; i++ ) {
    		p = points.get(i);
    		if (readptr >= p.getOffset() && readptr < p.getOffset() + p.getLength()) {
    			break;
    		}
    	}
    	
    	if (p.getTargetURN().equals("urn:aff4:UnknownData")) {
    		throw new AddressResolutionException();
    	}
    	
    	Reader bd = volume.open(p.getTargetURN());
    	long offsetWithinSlice = readptr - p.getOffset();
    	long available_to_read = Math.min((long)(p.getLength() - offsetWithinSlice), length);
    	bd.seek(p.getTargetOffset() + offsetWithinSlice);
     	ByteBuffer res = bd.read(available_to_read);
     	return res;
     }
    
    public ByteBuffer read() throws IOException, ParseException {
    	return read(size);
    }
    
    public ByteBuffer read(long length) throws IOException, ParseException {        
        length = Math.min(size - readptr, length);
        
        ByteBuffer result = ByteBuffer.allocate((int)length); 
        while (length > 0) {
            ByteBuffer data = partial_read((int)length);
            if (data.limit()==0) {
                break;
            }

            readptr += data.limit();
            result.put(data);
            length -= data.limit();
        }

        return result;
    }
    
    public void seek(long offset) {
    	readptr = offset;
    }
    
    public void close() {
    	
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