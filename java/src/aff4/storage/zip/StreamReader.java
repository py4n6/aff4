package aff4.storage.zip;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import java.util.zip.Inflater;

import aff4.datamodel.Readable;
import aff4.datamodel.Reader;
import aff4.infomodel.QueryTools;
import de.schlichtherle.util.zip.ZipEntry;

public class StreamReader  implements Reader {
	Readable volume;

	int chunk_size = 32*1024;
	int chunks_in_segment = 2048;
	int compression = 8;
	String urn;
	int bevvy_number = 0;
	long size;
	long readptr;
	Inflater decompresser = new Inflater();
	List<Long> bevvyOffsets;
	long bevvy = -1;
	
	public StreamReader(Readable v, String u) throws IOException, ParseException {
		volume = v;
		this.urn = u;
	
		String chunk_size_s = QueryTools.queryValue(volume, null, urn, "chunk_size");
		if (chunk_size_s != null) {
			chunk_size = Integer.parseInt(chunk_size_s);
		} 
		String chunks_in_segment_s = QueryTools.queryValue(volume, null, urn, "chunks_in_segment");
		if (chunks_in_segment_s != null) {
			chunks_in_segment = Integer.parseInt(chunks_in_segment_s);
		} 
	
		String size_s = QueryTools.queryValue(volume, null, urn, "aff4:size");
		if (size_s != null) {
			size = Integer.parseInt(size_s);
		} 
		String compression_s = QueryTools.queryValue(volume, null, urn, "compression");
		if (compression_s != null) {
			compression = Integer.parseInt(compression_s);
		} 
	}
	
	void open(String path) {
		
	}
	
	String encode(String url) throws UnsupportedEncodingException {
		return URLEncoder.encode(url, "UTF-8").replaceAll("%2F", "/");
	}
	
	static public long bytesIntoUInt(byte[] bytes, int offset) {
		long l =
			(((long)bytes[offset]) & 0xff)
				| (((long)(bytes[offset + 1]) & 0xff) << 8)
				| (((long)(bytes[offset + 2]) & 0xff) << 16)
				| (((long)(bytes[offset + 3]) & 0xff) << 24);
		return l;
	}
	
    public int partial_read(ByteBuffer buf) throws IOException {
    	if (readptr == size) {
    		return -1;
    	}
    	int length = (int)Math.min((size - readptr), buf.limit() - buf.position());

    	byte[] chunkInputBuffer = new byte[chunk_size];
        int chunk_id = (int) (readptr / chunk_size);
        int chuck_offset = (int) (readptr % chunk_size);
        int available_to_read = Math.min((chunk_size - chuck_offset), length);

        long thisBevvy = chunk_id / chunks_in_segment;
        long chunk_in_bevy = chunk_id % chunks_in_segment;
            
        String bevy_urn = String.format("%s/%08d",urn, thisBevvy);
        
        try {
            if (thisBevvy != bevvy) {
            	int offsetsCount = Integer.parseInt(QueryTools.queryValue(volume, null, bevy_urn + ".idx", "aff4:size"));
            	Reader is = volume.open(bevy_urn + ".idx");
            	ByteBuffer offsetBuf = ByteBuffer.allocate(offsetsCount);
            	int res = is.read(offsetBuf);
            	bevvyOffsets = new ArrayList<Long>(offsetsCount/4);
            	for (int i=0; i < offsetsCount/4 ; i++) {
            		bevvyOffsets.add(bytesIntoUInt(offsetBuf.array(), i*4));
            	}
            	bevvy = thisBevvy;
            	is.close();
            }

        	long offset = bevvyOffsets.get((int)chunk_in_bevy);
        	long offset_end = bevvyOffsets.get((int)chunk_in_bevy + 1);
            length = Math.min((int)(offset_end - offset), available_to_read);

            Reader is = volume.open(bevy_urn);
            is.position((long)offset);
            int res = -1;
            if (compression != ZipEntry.STORED) {
            	//InflaterInputStream iis = new InflaterInputStream(is, new Inflater());
            	ByteBuffer compressedData = ByteBuffer.allocate((int)(offset_end - offset));
            	int readBytes = is.read(compressedData);

            	decompresser.reset();
           		decompresser.setInput(compressedData.array(), 0, (int)(offset_end - offset));

                int resultLength = decompresser.inflate(chunkInputBuffer);
                is.close();
                //decompresser.end();
                buf.put(chunkInputBuffer, (int) chuck_offset, (int)available_to_read);
                //buf.position(buf.position() + available_to_read);
                return available_to_read;
            	//res = iis.read(buf,0,(int)available_to_read);
            } else {
                //res = is.read(chunkInputBuffer);
            	ByteBuffer b = ByteBuffer.allocate(chunk_size);
            	int r =is.read(b);
            	is.close();
                b.put(b.array(), 0, chunk_size);
                return chunk_size;
            }
            

            

        } catch (Exception ex) {
        	System.err.println(ex.getMessage());

        }
        return -1;
    }
    
    /*
    public ByteBuffer partial_read(long length) throws IOException {
    	byte[] chunkInputBuffer = new byte[chunk_size];
        int chunk_id = (int) (readptr / chunk_size);
        int chuck_offset = (int) (readptr % chunk_size);
        long available_to_read = Math.min((long)(chunk_size - chuck_offset), length);

        long thisBevvy = chunk_id / chunks_in_segment;
        long chunk_in_bevy = chunk_id % chunks_in_segment;
            
        String bevy_urn = String.format("%s/%08d",urn, thisBevvy);
        
        try {
            if (thisBevvy != bevvy) {
            	//String segment= encode(bevy_urn) + ".idx";
            	String segment= bevy_urn + ".idx";
            	int offsetsCount = Integer.parseInt(QueryTools.queryValue(volume, null, bevy_urn + ".idx", "aff4:size"));
            	Reader is = volume.open(bevy_urn + ".idx");
            	ByteBuffer offsetBuf  = is.read(offsetsCount);
            	bevvyOffsets = new ArrayList<Long>(offsetsCount/4);
            	for (int i=0; i < offsetsCount/4 ; i++) {
            		bevvyOffsets.add(bytesIntoUInt(offsetBuf.array(), i*4));
            	}
            	bevvy = thisBevvy;
            	is.close();
            }

        	long offset = bevvyOffsets.get((int)chunk_in_bevy);
        	long offset_end = bevvyOffsets.get((int)chunk_in_bevy + 1);
            length = Math.min((offset_end - offset), available_to_read);

            Reader is = volume.open(bevy_urn);
            is.position((long)offset);
            int res = -1;
            if (compression != ZipEntry.STORED) {
            	//InflaterInputStream iis = new InflaterInputStream(is, new Inflater());
            	ByteBuffer compressedData = is.read(offset_end - offset);
            	int readBytes = compressedData.position();
            	decompresser.reset();
           		decompresser.setInput(compressedData.array(), 0, (int)(offset_end - offset));

                int resultLength = decompresser.inflate(chunkInputBuffer);
                is.close();
                //decompresser.end();
                return ByteBuffer.wrap(chunkInputBuffer, (int) chuck_offset, (int)available_to_read);
            	//res = iis.read(buf,0,(int)available_to_read);
            } else {
                //res = is.read(chunkInputBuffer);
            	ByteBuffer b = is.read(chunk_size);
            	is.close();
                return b;
            }
            

            

        } catch (Exception ex) {
        	System.err.println(ex.getMessage());

        }
        return null;
    }
 
    public ByteBuffer read() throws IOException {
    	return read(size);
    }
    
    */
    public int read(ByteBuffer buf) throws IOException {
        if (size - readptr == 0) {
        	return -1;
        }
    	int length = (int)Math.min((size - readptr), buf.limit() - buf.position());
    	int startPosition = buf.position();
    	int readCount = 0;
        
        while (length > 0) {
        	int read = partial_read(buf);
        	if (read == -1) {
        		if (readCount == 0) {
        			return -1;
        		}
        		buf.limit(buf.position());
        		buf.position(startPosition);
        		return readCount;
        	}
            readptr += read;
            length -= read;   
            readCount += read;
        } 
        buf.position(startPosition);
        buf.limit(readCount);

        return readCount;   	
    }
    
    /*
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
    */
	public void setChunksInSegment(int count) {
		chunks_in_segment = count;
	}
	
    public void position(long offset) {
    	readptr = offset;
    }
    
    public long position() {
    	return readptr;
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
