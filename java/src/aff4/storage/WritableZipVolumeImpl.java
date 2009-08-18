package aff4.storage;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.UUID;

import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QuadStore;
import aff4.infomodel.serialization.PropertiesWriter;
import de.schlichtherle.util.zip.ZipEntry;
import de.schlichtherle.util.zip.ZipOutputStream;

public class WritableZipVolumeImpl implements ZipVolume {
	String path;
	String volumeURN;
	ZipOutputStream zipfile = null;
	String archiveName = null;
	QuadStore quadStore = null;
	FileOutputStream zipStream = null;
	
	public WritableZipVolumeImpl(String p) throws IOException {
		path = p;
		int lastSlash = p.lastIndexOf(File.separator);
		archiveName = p.substring(lastSlash + 1);
		volumeURN = "urn:aff4:" + UUID.randomUUID().toString();
		zipStream = new FileOutputStream(path);
		zipfile = new ZipOutputStream(zipStream);
		quadStore = new QuadStore();
	}
	
	public String getURN() {
		if (volumeURN == null) {
			volumeURN = "urn:aff4:" + UUID.randomUUID().toString();
		}
		return volumeURN;
	}
	
	public void close() throws IOException {
		zipfile.setComment(getURN());
		//ZipFile zf = new ZipFile(path);
		add(volumeURN, volumeURN, "aff4:type", "zip_volume");
		add(volumeURN, volumeURN, "aff4:interface", "aff4:volume");
		add(volumeURN, volumeURN, "aff4:stored", archiveName);
		
		String name =  "properties";
		ZipEntry ze = new ZipEntry(name);
		zipfile.putNextEntry(ze);
		
		PropertiesWriter writer = new PropertiesWriter(volumeURN);
		String res = writer.write(quadStore.query(null, volumeURN, null, null));
		zipfile.write(res.getBytes());
		zipfile.closeEntry();
		zipfile.finish();
		zipfile.close();
		zipStream.close();
		
		//AppendableZipFile azs = new AppendableZipFile(path);
		//azs.setComment(volumeURN);
		//azs.close();
	}
	
	public void add(String graph, String subject, String property, String object) {
		quadStore.add(new Quad(graph, subject, property, object));
	}
	
	public QuadList query(String g, String s, String p, String o) throws IOException {
		return quadStore.query(g, s, p, o);
	}
	
	public String getPath() {
		return path;
	}
	
	public QuadStore getQuadStore() {
		return quadStore;
	}
	
	public AFF4SegmentOutputStream createOutputStream(String name, boolean compressed, long uncompresedSize) throws IOException {
		ZipEntry ze = new ZipEntry(name);
		if (compressed) {
			ze.setMethod(ZipEntry.DEFLATED);
			zipfile.setMethod(ZipEntry.DEFLATED);
			
		} else {
			ze.setMethod(ZipEntry.STORED);
			zipfile.setMethod(ZipEntry.STORED);
		}
		
		
		return new AFF4SegmentOutputStream(ze,zipfile, uncompresedSize);
		
	}
	
	public StreamWriter open() {
		StreamWriter sw = new StreamWriter(this, zipfile);
		return sw;
	}
	
	public ZipOutputStream getZipFile() {
		return zipfile;
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
