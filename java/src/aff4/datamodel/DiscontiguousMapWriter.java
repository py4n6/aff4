package aff4.datamodel;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.util.ArrayList;

import aff4.infomodel.AFFObject;
import aff4.infomodel.serialization.Point;
import aff4.infomodel.serialization.PropertiesWriter;
import aff4.storage.zip.WritableZipVolume;

public class DiscontiguousMapWriter extends AFFObject implements SeekableWriter {
	WritableZipVolume volume = null;
	Writer backingStore = null;
	ArrayList<Point> points = null;
	long writePtr = 0;
	Point currentPoint = null;
	
	public  DiscontiguousMapWriter(WritableZipVolume v, Writer backingStore) throws IOException {
		volume = v;
		this.backingStore = backingStore;
		points = new ArrayList<Point>();
		currentPoint = new Point(0, 0, this.backingStore.position(), backingStore.getURN());
	}
	
	public void seek(long s) throws IOException {
		//currentPoint.setLength(writePtr - currentPoint.getOffset());
		points.add(currentPoint);
		currentPoint = new Point(writePtr, s-writePtr, -1, "urn:aff4:UnknownData");
		points.add(currentPoint);
		currentPoint = new Point(s, 0, backingStore.position(), backingStore.getURN());
		writePtr = s;
		
	}

	public void close() throws FileNotFoundException, IOException {
		String URN = getURN();
		
		volume.add(URN + "/properties", URN, "aff4:size", Long.toString(writePtr));
		volume.add(URN + "/properties", URN, "aff4:type", "map");
		
		for (Point p : points) {
			String source = URN + "[" + p.getOffset() + ":" + p.getLength() + "]";
			String dest;
			if (!p.getTargetURN().equals("urn:aff4:UnknownData")) {
				dest = p.getTargetURN() + "[" + p.getTargetOffset() + ":" + p.getLength() + "]";
			} else {
				dest = p.getTargetURN() ;
			}
			volume.add(URN + "/properties", URN, "aff4:contains", source);
			volume.add(URN + "/properties", source, "aff4:sameAs", dest);
		}
		
		volume.add(URN + "/properties", URN, "aff4:stored", volume.getURN());
		
		String name =  URLEncoder.encode(URN, "UTF-8")	+ "/properties";
		
		PropertiesWriter writer = new PropertiesWriter(URN);
		String res = writer
				.write(volume.getQuadStore().query(URN + "/properties", null, null, null));
		OutputStream f = volume.createOutputStream(name, true, res.length());
		f.write(res.getBytes());
		f.close();
		
		backingStore.close();
		
	}

	public void flush() throws FileNotFoundException, IOException {
		backingStore.flush();
		
	}

	public void write(ByteBuffer data) throws FileNotFoundException,
			IOException {
		currentPoint.setLength(data.limit() + currentPoint.getLength());
		writePtr = writePtr+ data.limit();
		backingStore.write(data);

		
	}

	public long position() throws FileNotFoundException, IOException {
		return writePtr;
	}

}
