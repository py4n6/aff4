package aff4.storage.zip;

import java.io.IOException;
import java.io.InputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.text.ParseException;
import java.util.UUID;

import aff4.datamodel.Reader;
import aff4.infomodel.Literal;
import aff4.infomodel.Node;
import aff4.infomodel.QueryTools;
import aff4.infomodel.Resource;
import aff4.infomodel.lexicon.AFF4;

public class RawReader  implements Reader {
	ReadOnlyZipVolume volume;

	Resource urn;
	int bevvy_number = 0;
	long size;
	InputStream stream = null;
	long readPtr = 0;
	
	public RawReader(ReadOnlyZipVolume v, Resource u) throws IOException, ParseException {
		volume = v;
		this.urn = u;
	
		long size = ((Literal)QueryTools.queryValue(volume, Node.ANY, urn, AFF4.size)).asLong();
		stream = v.getZipFile().getInputStream(encode(u.getURI()));
	}
	
	public RawReader(ReadOnlyZipVolume v, Resource u, long size) throws IOException, ParseException {
		volume = v;
		this.urn = u;
	
		this.size = size;
		stream = v.getZipFile().getInputStream(encode(u.getURI()));
	}
	
	void open(String path) {
		
	}
	
	String encode(String url) throws UnsupportedEncodingException{
		return URLEncoder.encode(url, "UTF-8").replaceAll("%2F", "/");
	}
	

   
    public int read(ByteBuffer buf) throws IOException {        
    	long length = buf.limit() - buf.position();
    	int originalPostition =  buf.position();
        if (length == 0) {
        	return 0;
        }
        int offset = buf.position();
        int readCount = 0;
        while (length > 0) {
            int read = stream.read(buf.array(), offset, (int)length);
            if (read == -1) {
            	if (readCount == 0) {
            		return -1;
            	}
            	buf.position(originalPostition);
            	return readCount;
            }
            offset += read;
            length -= read;
            readCount += read;
            readPtr += read;
        } 
        buf.position(originalPostition);
        return readCount;
    }
    

    
    public void position(long offset) throws IOException {
    	stream.skip(offset);
    	readPtr += offset;
    }
    
    public void close() throws IOException {
    	stream.close();
    	
    }
    
	public Resource getURN() {
		if (urn == null) {
			urn = new Resource("urn:aff4:" + UUID.randomUUID().toString());
		}
		return urn;
	}

	public long position() {
		return readPtr;
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
