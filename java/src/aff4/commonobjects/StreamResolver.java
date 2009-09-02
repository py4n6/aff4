package aff4.commonobjects;

import java.io.IOException;
import java.net.URLEncoder;
import java.util.Set;

import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.lexicon.AFF4;
import aff4.storage.zip.ReadOnlyZipVolume;
import de.schlichtherle.util.zip.ZipEntry;

public class StreamResolver {
	Set<ZipEntry> otherEntries = null;
	ReadOnlyZipVolume vol;
	
	public StreamResolver(ReadOnlyZipVolume vol, Set<ZipEntry> otherEntries) {
		this.vol = vol;
		this.otherEntries = otherEntries;
	}
	
	public QuadList query(Node g, Node s, Node p, Node o) throws IOException {
		String prefix = URLEncoder.encode(s.getURI(), "UTF-8");
		QuadList results = new QuadList();
		for (ZipEntry e : otherEntries) {
			if (e.getName().startsWith(prefix)) {
				String rest = e.getName().substring(prefix.length(), e.getName().length());
				String[] res = rest.split("/\\//");
				if ( res.length == 2 || res.length == 1) {
					// this is a direct child
					if (rest.endsWith(".idx")) {
						results.add(new Quad(vol.getGraph(), s, AFF4.containsSegmentIndex, Node.createLiteral(e.getName(), null, null)));
					} else {
						results.add(new Quad(vol.getGraph(), s, AFF4.containsSegment, Node.createLiteral(e.getName(), null,null)));
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