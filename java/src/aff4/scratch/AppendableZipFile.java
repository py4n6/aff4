package aff4.scratch;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.zip.ZipException;

import de.schlichtherle.io.util.LEDataOutputStream;
import de.schlichtherle.util.zip.ZipEntry;
import de.schlichtherle.util.zip.ZipFile;

public class AppendableZipFile {
	AppendableZipOutputStream azf = null;
	
	public AppendableZipFile(String path) throws NullPointerException, FileNotFoundException, ZipException, IOException {
		ZipFile zf = new ZipFile(path);
		Enumeration<ZipEntry> e = zf.entries();
		ArrayList<ZipEntry> entries = new ArrayList<ZipEntry>();
		while (e.hasMoreElements()) {
			entries.add(e.nextElement());
		}
		zf.close();
		
		java.io.RandomAccessFile f = new java.io.RandomAccessFile(path, "rw");
		SeekableOutputStream bos = new SeekableOutputStream(f);
		f.seek(zf.getCDoffset());
		LEDataOutputStream ds = new LEDataOutputStream(bos, (int)zf.getCDoffset());
		azf = new AppendableZipOutputStream(ds, entries);
	}
	
	public void setComment(String comment) {
		azf.setComment(comment);
	}
	
	public void flush() throws IOException {
		azf.flush();
	}
	
	public void close() throws IOException {
		azf.close();
		
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
