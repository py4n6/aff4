package aff4.infomodel.serialization;

import aff4.infomodel.Resource;

public class Point {
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

	public long getTargetOffset() {
		return targetOffset;
	}

	public void setTargetOffset(long targetOffset) {
		this.targetOffset = targetOffset;
	}

	public Resource getTargetURN() {
		return targetURN;
	}

	public void setTargetURN(Resource targetURN) {
		this.targetURN = targetURN;
	}

	long offset;
	long length;
	long targetOffset;
	Resource targetURN;
	
	public Point(long offset, long length, long targetOffset, Resource targetURN ) {
		this.offset = offset;
		this.length = length;
		this.targetOffset = targetOffset;
		this.targetURN = targetURN;
	}
	
	public String toString() {
		return "[" + offset + "," + length + "]=" + targetURN + "[" + targetOffset + "," + length + "]";
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