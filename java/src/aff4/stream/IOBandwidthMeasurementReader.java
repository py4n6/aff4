package aff4.stream;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.text.ParseException;

import aff4.datamodel.Reader;

public class IOBandwidthMeasurementReader implements Reader {
	Reader source = null;
	long countBytes = 0;
	long lastTime = 0;
	
	public IOBandwidthMeasurementReader(Reader child) {
		source = child;
		lastTime = System.currentTimeMillis();
	}
	
	public void close() throws IOException {
		source.close();
	}

	public void position(long s) throws IOException {
		source.position(s);
		
	}

	public long position() {
		return source.position();
	}

	public int read(ByteBuffer buf) throws IOException, ParseException {
		int res = source.read(buf);
		if (res != -1) {
			countBytes += res;
		}
		return res;
	}

	public float getBandwidthMBs() {
		long currentTime = System.currentTimeMillis();
		float durationSeconds = (currentTime - lastTime)/1000;
		float bandwidthBytes = (countBytes/(1024*1024))/durationSeconds;
		countBytes = 0;
		lastTime = currentTime;
		return bandwidthBytes;
	}
}
