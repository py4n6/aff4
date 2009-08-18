package aff4.storage;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.util.zip.CRC32;

import de.schlichtherle.util.zip.ZipEntry;
import de.schlichtherle.util.zip.ZipFile;
import de.schlichtherle.util.zip.ZipOutputStream;

public class AFF4SegmentOutputStream extends OutputStream {
	ZipOutputStream volume;
	ZipEntry entry = null;
	long uncompressedSize = 0;
	ByteBuffer buffer = null;
	int writePtr = 0;
	
	public AFF4SegmentOutputStream(ZipEntry ze , ZipOutputStream zipfile, long uncompressedSize) {
		this.volume = zipfile;
		entry = ze;
		this.uncompressedSize = uncompressedSize;
		buffer = ByteBuffer.allocate((int)uncompressedSize);
		ze.setSize(uncompressedSize);
	}

	@Override
	public void write(int b) throws IOException {
		buffer.put((byte) b);
	}

	
	@Override
	public void flush() throws IOException {
		// TODO Auto-generated method stub
		volume.flush();
	}

	@Override
	public void write(byte[] b, int off, int len) throws IOException {
		buffer.put(b, off, len);
		
	}

	@Override
	public void write(byte[] b) throws IOException {
		// TODO Auto-generated method stub
		buffer.put(b);
	}

	public void close() throws IOException {
		entry.setCompressedSize(buffer.position());
		CRC32 crc = new CRC32();
		crc.update(buffer.array(), 0, buffer.position());
		entry.setCrc(crc.getValue());
		volume.putNextEntry(entry);
		volume.write(buffer.array(), 0, buffer.position());
		volume.closeEntry();
	}
}
