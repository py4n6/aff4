package aff4.stream;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;

import aff4.datamodel.Readable;
import aff4.infomodel.QueryTools;
import aff4.infomodel.TooManyValuesException;
import aff4.infomodel.resource.Resource;
import aff4.infomodel.resource.ResourceParser;
import aff4.infomodel.resource.SliceResource;
import aff4.storage.zip.StreamReader;

public class DataObjectVisitOrchestrator {
	
	HashMap<String,List<SequentialPath>> orderedPaths = null;
	Readable volume;
	String urn = null;
	
	long lastOffset = 0;
	int validatedCount = 0;
	
	public DataObjectVisitOrchestrator(Readable volume) {
		orderedPaths = new HashMap<String,List<SequentialPath>>();
		this.volume = volume;
	}
	
	public void add(String dataObjectURN, OrderedDataStreamVisitor visitor) throws ParseException, NumberFormatException, TooManyValuesException, IOException {
		Resource r = ResourceParser.parse(dataObjectURN);
		urn = r.getBaseURN();
		
		if (!orderedPaths.containsKey(urn)) {
			orderedPaths.put(urn, new ArrayList<SequentialPath>());
		}
		List<SequentialPath> pathList = orderedPaths.get(urn);
		
		
		if (r instanceof SliceResource) {
			SliceResource rr = (SliceResource) r;
			pathList.add(new SequentialPath(rr.getOffset(), rr.getLength(), visitor));
		} else {
			long size = Long.parseLong(QueryTools.queryValue(volume, null, dataObjectURN, "aff4:size"));
			pathList.add(new SequentialPath(0, size, visitor));
		}
	}
	
	public void run() throws IOException, ParseException{
		for (String deviceURN : orderedPaths.keySet()) {
			runDevice(deviceURN, orderedPaths.get(deviceURN));
		}
	}
	
	void visitorValidated(long offset, long length) {
		validatedCount +=1;
	}
	
	void summarise(IOBandwidthMeasurementReader reader, long offset) {
		System.out.println("[" + lastOffset + "," + offset + "] : verified " + validatedCount + " hash chunks @ " + reader.getBandwidthMBs() + " MB/s");
		lastOffset = offset;
	}
		
	
	void runDevice(String deviceUrn, List<SequentialPath> paths) throws IOException, ParseException{
		int cursor = 0;
		long readPtr = 0;
		Collections.sort(paths);
		
		long lastTime = System.currentTimeMillis();
		
		IOBandwidthMeasurementReader reader = new IOBandwidthMeasurementReader(new StreamReader(volume, urn));

		if (paths.size() > 1) {
			readPtr = paths.get(cursor).start / 64*1024;
		}
		reader.position(readPtr);
		ByteBuffer data = ByteBuffer.allocate(64*1024);
		while (paths.size() > 0) {
			int res = reader.read(data);
			if (System.currentTimeMillis() - lastTime > 5*1000) {
				lastTime = System.currentTimeMillis();
				summarise(reader, readPtr);
			}
			if (res == -1) {
				throw new IOException();
			}
			int localCursor = cursor;
			while (!paths.isEmpty() && paths.get(localCursor) != null && paths.get(localCursor).contains(readPtr)) {
				SequentialPath element = paths.get(localCursor);
				int dataPosition = data.position();
				int dataLimit = data.limit();
				
				int offsetWithinData = (int) (element.start % (64*1024));
				data.position(offsetWithinData);
				element.visitor.visit(data);
				data.position(dataPosition);
				if (element.start + element.length <= readPtr + data.limit()) {
					// this one has been satified
					paths.remove(localCursor);
					if (element.visitor.finish(element.start, element.length)) {
						visitorValidated(element.start, element.length);
					}
				} else {
					localCursor += 1;
				}
			}
			readPtr += data.limit();
		}

	}
	
	public String toString() {
		StringBuffer buf = new StringBuffer();
		for (String deviceURN : orderedPaths.keySet()) {
			for (SequentialPath p : orderedPaths.get(deviceURN)) {
				buf.append(p);
				buf.append("\r\n");
			}
		}

		return buf.toString();
	}
}
