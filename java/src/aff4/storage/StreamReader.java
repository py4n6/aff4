package aff4.storage;

import java.io.IOException;
import java.io.InputStream;
import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.text.ParseException;
import java.util.List;
import java.util.UUID;
import java.util.zip.Inflater;

import aff4.util.StructConverter;
import de.schlichtherle.util.zip.ZipEntry;

public class StreamReader  implements Reader {
	ReadOnlyZipVolume volume;
	List<String> segments;
	List<String> indexes;
	int chunk_size = 32*1024;
	int chunks_in_segment = 2048;
	int compression = 8;
	String urn;
	int bevvy_number = 0;
	long size;
	long readptr;
	Inflater decompresser = new Inflater();
	
	public StreamReader(ReadOnlyZipVolume v, String u) throws IOException, ParseException {
		volume = v;
		this.urn = u;
	
		segments = volume.queryValues(null, urn, "aff4:containsSegment");
		indexes = volume.queryValues(null, urn, "aff4:containsSegmentIndex");
		String chunk_size_s = volume.queryValue(null, urn, "chunk_size");
		if (chunk_size_s != null) {
			chunk_size = Integer.parseInt(chunk_size_s);
		} 
		String chunks_in_segment_s = volume.queryValue(null, urn, "chunks_in_segment");
		if (chunks_in_segment_s != null) {
			chunks_in_segment = Integer.parseInt(chunks_in_segment_s);
		} 
	
		String size_s = volume.queryValue(null, urn, "aff4:size");
		if (size_s != null) {
			size = Integer.parseInt(size_s);
		} 
		String compression_s = volume.queryValue(null, urn, "compression");
		if (compression_s != null) {
			compression = Integer.parseInt(compression_s);
		} 
	}
	
	void open(String path) {
		
	}
	
	String encode(String url) {
		return URLEncoder.encode(url).replaceAll("%2F", "/");
	}
	
    public ByteBuffer partial_read(long length) throws IOException {
    	byte[] buf = new byte[chunk_size];
        int chunk_id = (int) (readptr / chunk_size);
        int chuck_offset = (int) (readptr % chunk_size);
        long available_to_read = Math.min((long)(chunk_size - chuck_offset), length);

        long bevy = chunk_id / chunks_in_segment;
        long chunk_in_bevy = chunk_id % chunks_in_segment;
            
        String bevy_urn = String.format("%s/%08d",urn, bevy);
        
        try {
        	String segment= encode(bevy_urn) + ".idx";
        	InputStream is = volume.zf.getInputStream(segment);
        	
        	is.skip((int)(4 * chunk_in_bevy));
        	is.read(buf,0,8);
        	ByteBuffer bb = ByteBuffer.wrap(buf);
        	long offset = StructConverter.bytesIntoUInt(buf, 0);
        	long offset_end = StructConverter.bytesIntoUInt(buf, 4);
            length = Math.min((offset_end - offset), available_to_read);
        	//length = offset_end - offset;
            is.close();

            //ZipEntry ee = volume.zf.getEntry(encode(bevy_urn));
            
            is = volume.zf.getInputStream(encode(bevy_urn));
            long skipped = is.skip(offset);
            int res = -1;
            if (compression != ZipEntry.STORED) {
            	//InflaterInputStream iis = new InflaterInputStream(is, new Inflater());
            	byte[] cdata = new byte[(int)(offset_end - offset)];
            	int readBytes = is.read(cdata);
            	decompresser.reset();
           		decompresser.setInput(cdata, 0, (int)(offset_end - offset));

                int resultLength = decompresser.inflate(buf);
                //decompresser.end();
          	
            	//res = iis.read(buf,0,(int)available_to_read);
            } else {
                res = is.read(buf);
            }
            is.close();
            //System.out.println(HexFormatter.convertBytesToString(buf));
            return ByteBuffer.wrap(buf, (int) chuck_offset, (int)available_to_read);
        } catch (Exception ex) {
        	System.err.println(ex.getMessage());

        }
        return null;
        
    }
        
    public ByteBuffer read() throws IOException {
    	return read(size);
    }
    
    public ByteBuffer read(long length) throws IOException {        
        length = Math.min(size - readptr, length);
        if (length == 0) {
        	return null;
        }
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
        result.position(0);
        return result;
    }
    
	public void setChunksInSegment(int count) {
		chunks_in_segment = count;
	}
	
    public void seek(long offset) {
    	readptr = offset;
    }
    
    public void close() {
    	
    }
    
	public String getURN() {
		if (urn == null) {
			urn = "urn:aff4:" + UUID.randomUUID().toString();
		}
		return urn;
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
