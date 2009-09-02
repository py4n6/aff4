package aff4.storage.zip;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.util.UUID;
import java.util.zip.Deflater;

import org.bouncycastle.crypto.digests.MD5Digest;

import aff4.datamodel.Writer;
import aff4.hash.HashDigestAdapter;
import aff4.infomodel.Node;
import aff4.infomodel.Resource;
import aff4.infomodel.datatypes.AFF4Datatype;
import aff4.infomodel.datatypes.DataType;
import aff4.infomodel.lexicon.AFF4;
import aff4.infomodel.serialization.PropertiesWriter;
import aff4.util.StructConverter;
import de.schlichtherle.util.zip.ZipOutputStream;

public class StreamWriter implements Writer {
	WritableZipVolume volume = null;
	Resource URN = null;
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
	boolean closed = false;
	ZipOutputStream zipfile = null;
	HashDigestAdapter hash = null;
	
	public StreamWriter(WritableZipVolume v, ZipOutputStream zipfile) {
		URN = Node.createURI("urn:aff4:" + UUID.randomUUID().toString());
		volume = v;
		this.zipfile = zipfile;

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
		hash = new HashDigestAdapter(new MD5Digest());
	}

	public void setChunksInSegment(int count) {
		chunks_in_segment = count;
		bevy_size = chunk_size * chunks_in_segment;
		buf = ByteBuffer.allocate((int) bevy_size);
		bevvy = ByteBuffer.allocate((int) bevy_size);
		bevvy_index = ByteBuffer.allocate((chunks_in_segment + 2) * 4);
	}
	
	public void write(ByteBuffer data) throws FileNotFoundException,
			IOException {
		data.position(0);
		available_to_read = Math.min(bevy_size - buf.position(), data
				.capacity()
				- data.position());
		data.limit((int) available_to_read);
		buf.put(data);
		data.position(0);
		hash.update(data);
		writeptr = writeptr + available_to_read;
		
		if (buf.position() == buf.limit()) {
			buf.position(0);
			writeBevvy(buf.limit());
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
		if (!flushed && !closed) {
			flush();
		}
		
		if (! closed) {
			flushed = true;
			hash.doFinal();
			volume.add(volume.getGraph(), URN, AFF4.size, Node.createLiteral(Long.toString(writeptr), null, DataType.integer));
			volume.add(volume.getGraph(), URN, AFF4.type, AFF4.image);
			volume.add(volume.getGraph(), URN, AFF4.iface, AFF4.stream);
			volume.add(volume.getGraph(), URN, AFF4.stored, volume.getURN());
			volume.add(volume.getGraph(), URN, AFF4.hash, Node.createLiteral(hash.getStringValue(), null, AFF4Datatype.md5));
			volume.add(volume.getGraph(), volume.getURN(), AFF4.contains, Node.createURI(URN.getURI() + "/properties"));
			
			/*
			String name =  URLEncoder.encode(URN.getURI(), "UTF-8")
					+ "/properties";
			
			
			PropertiesWriter writer = new PropertiesWriter(URN);
			String res = writer
					.write(volume.quadStore.query(null, URN, null, null));
			OutputStream os = volume.createOutputStream(name, true, res.length());
			os.write(res.getBytes());
			os.close();
			*/
			closed = true;
		}
	}

	void writeBevvy(long limit) throws FileNotFoundException, IOException {
		Deflater d = new Deflater();
		int pos = 0; 

		long offset_end = 0;
		int len = 0;
		byte[] outBuf = new byte[chunk_size*2];
		byte[] b = StructConverter.UInt2bytes(offset_end);
		bevvy_index.put(b);
		while (pos < limit) {
			
			//buf.limit(buf.position() + chunk_size);
			if (compression > 0) {
				byte[] input = buf.array();
				int count = Math.min(chunk_size, (int)(limit - pos));
				d.reset();
				d.setInput(input, pos, count);
				d.finish();
				d.deflate(outBuf);
				len = (int) d.getBytesWritten();
				if (!d.finished()) {
					throw new RuntimeException("Decompression buffer too small");
				}
				
				bevvy.put(outBuf, 0,  len);
				//len = (int) off;

			} else {
				bevvy.put(buf.array());
				len = chunk_size;
			}

			offset_end = offset_end + len;
			b = StructConverter.UInt2bytes(offset_end);

			bevvy_index.put(b);
			pos = pos + chunk_size;
			//buf.position((int) offset);
		}
		bevvy_index.putInt(-1);
		String subject = URLEncoder.encode(URN.getURI(), "UTF-8") + "/"
				+ String.format("%1$08d", bevvy_number);
		String bevvyFileName = subject;
		String bevvyIndexFileName = subject + ".idx";

		OutputStream f = volume.createOutputStream(bevvyFileName,false, bevvy.position());
		f.write(bevvy.array(), 0, bevvy.position());
		f.close();
		
		// the following informaiton is currently required by Michael's implementations
		volume.add(volume.getGraph(), volume.getURN(), AFF4.contains, Node.createURI(URN.getURI() + "/" + String.format("%1$08d", bevvy_number)));
		volume.add(volume.getGraph(), volume.getURN(), AFF4.contains, Node.createURI(URN.getURI() + "/" + String.format("%1$08d", bevvy_number) + ".idx"));
		
		f = volume.createOutputStream(bevvyIndexFileName,true,bevvy_index.position());
		f.write(bevvy_index.array(), 0, bevvy_index.position());
		f.close();

		bevvy_number = bevvy_number +1;
		buf.position(0);
		bevvy.position(0);
		bevvy_index.position(0);
		flushed = true;
	}

	public Resource getURN() {
		return URN;
	}
	
	public long getSize() {
		return writeptr;
		
	}
	
	public long position() {
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
