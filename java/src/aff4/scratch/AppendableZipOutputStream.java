package aff4.scratch;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.util.List;
import java.util.zip.ZipException;

import de.schlichtherle.io.rof.ReadOnlyFile;
import de.schlichtherle.io.util.LEDataOutputStream;
import de.schlichtherle.util.zip.ZipEntry;
import de.schlichtherle.util.zip.ZipFile;
import de.schlichtherle.util.zip.ZipOutputStream;

public class AppendableZipOutputStream extends ZipOutputStream {

	public AppendableZipOutputStream(OutputStream os, List<ZipEntry> existingEntries) throws NullPointerException,
			FileNotFoundException, ZipException, IOException {
		super(os);
		for (ZipEntry e : existingEntries) {
			entries.put(e.getName(), e);
		}
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