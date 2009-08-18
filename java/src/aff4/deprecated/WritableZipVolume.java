package aff4.deprecated;

import java.io.IOException;
import java.net.URLEncoder;
import java.util.UUID;

import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QuadStore;
import aff4.infomodel.serialization.PropertiesWriter;
import aff4.scratch.AppendableZipFile;
import aff4.storage.ZipVolume;

import de.schlichtherle.io.ArchiveException;
import de.schlichtherle.io.File;
import de.schlichtherle.io.FileWriter;
import de.schlichtherle.util.zip.ZipFile;
import de.schlichtherle.util.zip.ZipOutputStream;

public class WritableZipVolume implements ZipVolume {
	String path;
	String volumeURN;
	File zipfile = null;
	String archiveName = null;
	QuadStore quadStore = null;
	
	public WritableZipVolume(String p) throws IOException {
		path = p;
		int lastSlash = p.lastIndexOf(File.separator);
		archiveName = p.substring(lastSlash + 1);
		volumeURN = "urn:aff4:" + UUID.randomUUID().toString();
		zipfile = new File(path);
		quadStore = new QuadStore();
	}
	
	public String getURN() {
		if (volumeURN == null) {
			volumeURN = "urn:aff4:" + UUID.randomUUID().toString();
		}
		return volumeURN;
	}
	public void close() throws IOException {
		//ZipFile zf = new ZipFile(path);
		add(volumeURN, volumeURN, "aff4:type", "zip_volume");
		add(volumeURN, volumeURN, "aff4:interface", "aff4:volume");
		add(volumeURN, volumeURN, "aff4:stored", archiveName);
		
		String name =  path + "/" + "/properties";
		FileWriter f = new FileWriter(name);
		PropertiesWriter writer = new PropertiesWriter(volumeURN);
		String res = writer.write(quadStore.query(null, volumeURN, null, null));
		f.write(res);
		f.close();
		
		try {
			zipfile.umount();
		} catch (ArchiveException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		AppendableZipFile azs = new AppendableZipFile(path);
		azs.setComment(volumeURN);
		azs.close();
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
