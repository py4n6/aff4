package aff4.infomodel;

public class SliceResource extends Resource {
	public long getOffset() {
		return offset;
	}

	public void setOffset(long offset) {
		this.offset = offset;
	}

	public long getLength() {
		return length;
	}

	public void setLength(long length) {
		this.length = length;
	}

	long offset;
	long length;
	String baseURI;
	
	public SliceResource(String u, long o, long l) {
		super(u);
		offset = o;
		length = l;
		int fragmentLoc = u.indexOf("#");
		baseURI = u.substring(0, fragmentLoc);
	}
	
	public SliceResource(String ns, String local, long o, long l) {
		super(ns, local);
		offset = o;
		length = l;
	}
	
	public Resource clone() {
		if (URI != null) {
			return new SliceResource(URI, offset, length);
		}
		if (localName != null && nameSpace != null) {
			return new SliceResource(nameSpace, localName, offset, length);
		}
		
		return null;
	}
	
	public String getURI() {
		if (nameSpace != null && localName != null) {
			return nameSpace + ":" + localName;
		} else {
			return URI;
		}
	}
	
	public String getBaseURI() {
		return baseURI;
		
	}
	
	public String getFullURI() {
		return URI;
	}
	
	public boolean equals(Object o) {
		
		if (! (o instanceof SliceResource)) {
			return false;
		}
		SliceResource r = (SliceResource) o;
		if (URI != r.URI || offset != r.offset || length != r.length || nameSpace != r.nameSpace || localName != r.localName)
			return false;

		return true;
	}

	public int compareTo(Resource o) {
		if (o instanceof SliceResource) {
			SliceResource sr = (SliceResource) o;
			if (sr.baseURI.equals(this.baseURI)) {
				if (this.offset < sr.offset) {
					return -1;
				} else if (this.offset > sr.offset) {
					return 1;
				} else {
					return 0;
				}
			} else {
				return this.baseURI.compareTo(sr.baseURI);
			}
		} else {
			return getURI().compareTo(o.getURI());
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