package aff4.datamodel;

import java.io.IOException;

import aff4.infomodel.Resource;
import aff4.storage.zip.AFF4SegmentOutputStream;
import aff4.storage.zip.StreamWriter;

public interface Writable {
	public AFF4SegmentOutputStream createOutputStream(String name, boolean compressed, long uncompresedSize) throws IOException;
	public StreamWriter createWriter() throws IOException;
	public Resource getGraph();
}
