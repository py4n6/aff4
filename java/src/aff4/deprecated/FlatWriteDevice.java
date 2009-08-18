package aff4.deprecated;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.util.UUID;
import java.util.zip.Deflater;

import de.schlichtherle.io.File;
import de.schlichtherle.io.FileOutputStream;
import de.schlichtherle.io.FileWriter;

import aff4.infomodel.serialization.PropertiesWriter;
import aff4.util.StructConverter;

public class FlatWriteDevice {
	WritableZipVolume volume = null;
	String URN = null;
	int chunk_size = 32 * 1024;
	int chunks_in_segment = 2048;
	int compression = 8;
	int bevvy_number = 0;
	long size;
	long writeptr = 0;
	long available_to_read = 0;
	ByteBuffer buf = null;
	long bevy_size;
	ByteBuffer bevvy = null;
	ByteBuffer bevvy_index = null;
	boolean flushed = false;

	public FlatWriteDevice(WritableZipVolume v) {
		URN = "urn:aff4:" + UUID.randomUUID().toString();
		volume = v;

		/*
		 * String chunk_size_s = volume.queryValue(urn, "chunk_size"); if
		 * (chunk_size_s != null) { chunk_size = Integer.parseInt(chunk_size_s);
		 * } String chunks_in_segment_s = volume.queryValue(urn,
		 * "chunks_in_segment"); if (chunks_in_segment_s != null) {
		 * chunks_in_segment = Integer.parseInt(chunks_in_segment_s); }
		 */

		bevy_size = chunk_size * chunks_in_segment;
		buf = ByteBuffer.allocate((int) bevy_size);
		bevvy = ByteBuffer.allocate((int) bevy_size);
		bevvy_index = ByteBuffer.allocate((chunks_in_segment + 2) * 4);
	}

	public void write(ByteBuffer data) throws FileNotFoundException,
			IOException {

		available_to_read = Math.min(bevy_size - buf.position(), data
				.capacity()
				- data.position());
		data.limit((int) available_to_read);
		buf.put(data);
		writeptr = writeptr + available_to_read;

		if (buf.position() == buf.limit()) {
			writeBevvy(buf.position());
		} else {
			flushed = false;
		}
	}

	public void flush() throws FileNotFoundException, IOException {
		if (!flushed) {
			writeBevvy(writeptr % bevy_size);
		}
	}

	public void close() throws FileNotFoundException, IOException {
		if (!flushed) {
			flush();
		}
		flushed = true;
		volume.add(URN + "/properties", URN, "aff4:size", Long.toString(writeptr));
		volume.add(URN + "/properties", URN, "aff4:type", "image");
		volume.add(URN + "/properties", URN, "aff4:interface", "aff4:stream");
		volume.add(URN + "/properties", URN, "aff4:stored", volume.getURN());
		volume.add(volume.getURN(), volume.getURN(), "aff4:contains", URN + "/properties");
		
		String name = volume.path + "/" + URLEncoder.encode(URN)
				+ "/properties";
		FileWriter f = new FileWriter(name);
		PropertiesWriter writer = new PropertiesWriter(URN);
		String res = writer
				.write(volume.quadStore.query(null, URN, null, null));
		f.write(res);
		f.close();
	}

	void writeBevvy(long limit) throws FileNotFoundException, IOException {
		Deflater d = new Deflater();
		int pos = buf.position();
		buf.position(0);
		buf.limit(pos);
		long offset = 0;
		long offset_end = 0;
		int len = 0;
		byte[] outBuf = new byte[pos];
		byte[] b = StructConverter.UInt2bytes(offset_end);
		bevvy_index.put(b);
		while (buf.position() != limit) {
			buf.limit(buf.position() + chunk_size);
			if (compression > 0) {
				byte[] input = buf.array();
				d.setInput(input, buf.position(), buf.limit() - buf.position());
				d.finish();
				d.deflate(outBuf);
				long off = d.getBytesWritten();
				bevvy.put(outBuf, 0, (int) off);
				len = (int) off;

			} else {
				bevvy.put(buf.array());
				len = buf.limit();
			}

			offset_end = offset_end + len;
			b = StructConverter.UInt2bytes(offset_end);
			bevvy_index.put(b);
			offset = offset + chunk_size;
			buf.position((int) offset);
		}
		bevvy_index.putInt(-1);
		String subject = URLEncoder.encode(URN) + "/"
				+ String.format("%1$08d", bevvy_number);
		String bevvyFileName = volume.path + "/" + subject;
		String bevvyIndexFileName = volume.path + "/" + subject + ".idx";

		FileOutputStream f = new FileOutputStream(bevvyFileName);
		f.write(bevvy.array(), 0, bevvy.position());
		f.close();
		
		// the following informaiton is currently required by Michael's implementations
		volume.add(volume.getURN(), volume.getURN(), "aff4:contains", URN + "/" + subject);
		volume.add(volume.getURN(), volume.getURN(), "aff4:contains", URN + "/" + subject + ".idx");
		
		f = new FileOutputStream(bevvyIndexFileName);
		f.write(bevvy_index.array(), 0, bevvy_index.position());
		f.close();

		bevvy_number = +1;
		buf.position(0);
		bevvy.position(0);
		bevvy_index.position(0);
		flushed = true;
	}

	public String getURN() {
		return URN;
	}
	
	public long getSize() {
		return writeptr;
		
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
