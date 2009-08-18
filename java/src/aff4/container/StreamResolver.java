package aff4.container;

import java.io.IOException;
import java.net.URLEncoder;
import java.util.Set;

import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.storage.ReadOnlyZipVolume;
import de.schlichtherle.util.zip.ZipEntry;

public class StreamResolver {
	Set<ZipEntry> otherEntries = null;
	ReadOnlyZipVolume vol;
	
	public StreamResolver(ReadOnlyZipVolume vol, Set<ZipEntry> otherEntries) {
		this.vol = vol;
		this.otherEntries = otherEntries;
	}
	
	public QuadList query(String g, String s, String p, String o) throws IOException {
		String prefix = URLEncoder.encode(s);
		QuadList results = new QuadList();
		for (ZipEntry e : otherEntries) {
			if (e.getName().startsWith(prefix)) {
				String rest = e.getName().substring(prefix.length(), e.getName().length());
				String[] res = rest.split("/\\//");
				if ( res.length == 2 || res.length == 1) {
					// this is a direct child
					if (rest.endsWith(".idx")) {
						results.add(new Quad(vol.getURN(), s, "aff4:containsSegmentIndex", e.getName()));
					} else {
						results.add(new Quad(vol.getURN(), s, "aff4:containsSegment", e.getName()));
					}
				}
			}
		}
		return results;
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