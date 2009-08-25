package aff4.datamodel;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.text.ParseException;
import java.util.ArrayList;

import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QueryTools;
import aff4.infomodel.resource.ResourceParser;
import aff4.infomodel.resource.SliceResource;
import aff4.infomodel.serialization.Point;

public class MapReader implements Reader{
	ArrayList<Point> points = null;
	String URN = null;
	long size;
	long readptr;
	Readable volume = null;
	
	public MapReader(Readable v, String u) throws IOException, ParseException {
		URN = u;
		volume = v;
		points = new ArrayList<Point>();
		QuadList res = v.query(null, URN, "aff4:contains", null);
		for (Quad q : res) {
			SliceResource part = ResourceParser.parseSlice(q.getObject());
			String targetStr = QueryTools.queryValue(v, null, q.getObject(), "aff4:sameAs");
			Point p = null;
			if (targetStr.equals("urn:aff4:UnknownData")) {
				p = new Point(part.getOffset(), part.getLength(), -1,"urn:aff4:UnknownData");
			} else {
				SliceResource target = ResourceParser.parseSlice(targetStr);
				p = new Point(part.getOffset(), part.getLength(), target.getOffset(), target.getBaseURN());
			}
			points.add(p);
		}
		
		size = Long.parseLong(QueryTools.queryValue(v, null, URN, "aff4:size"));
		readptr = 0;
	}
	
	/*
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
    	bd.position(p.getTargetOffset() + offsetWithinSlice);
     	ByteBuffer res = bd.read(available_to_read);
     	return res;
     }
    */
    public int partial_read(ByteBuffer buf) throws IOException, ParseException {
    	long length = buf.limit() - buf.position();
    	int oldLimit = buf.limit();
    	Point p = null;
    	
    	int i;
    	// TODO - really inefficient
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
    	bd.position(p.getTargetOffset() + offsetWithinSlice);
    	buf.limit((int)(buf.position() + available_to_read));
     	int read = bd.read(buf);
     	buf.limit(oldLimit);
     	return read;
     }
    

    
    public int read(ByteBuffer buf) throws IOException, ParseException {
    	int length = buf.capacity() - buf.position();
    	int readCount = 0;
        if (length == 0) {
        	return 0;
        }
        
        while (length > 0) {
        	int read = partial_read(buf);
        	if (read == -1) {
        		return -1;
        	}
            readptr += read;
            length -= read;   
            readCount += read;
        } 
        buf.position(0);
        buf.limit(readCount);
        return readCount;   	
    }
    
    /*
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
    */
    public void  position(long offset) {
    	readptr = offset;
    }
    
    public void close() {
    	
    }

	public long position() {
		return readptr;
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