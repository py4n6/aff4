package aff4.infomodel.serialization;

import java.io.IOException;
import java.io.InputStream;
import java.text.ParseException;
import java.util.ArrayList;

import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.Resource;
import aff4.infomodel.lexicon.AFF4;

public class MapResolver extends PropertiesReader {
	

	
	ArrayList<Point> result = null;
	Node p;
	Node o;
	long lastOffset = 0;
	boolean firstLine = true;
	Point currentPoint = null;
	long streamSize = -1;
	Resource selfURN;
	
	public MapResolver(Resource selfURN, long size, InputStream stream) {
		super(stream);
		this.selfURN = selfURN;
		this.streamSize = size;
	}
	
	public QuadList query(Node s, Node p, Node o) throws IOException, ParseException {
		result = new ArrayList<Point>();
		QuadList res = new QuadList();
		firstLine = true;
		currentPoint = null;
		this.p = p;
		this.o = o;
		process();
		finish();
		
		if ((p == null && o == null) || 
				((p.equals(AFF4.sameAs) || p.equals(AFF4.contains)) && o == null)) {
			for (Point point : result) {
				if (point.length != 0) {
					
					StringBuffer target = new StringBuffer();
					target.append(point.targetURN.getURI());
					target.append("[");
					target.append(point.targetOffset);
					target.append(",");
					target.append(point.length);
					target.append("]");

					StringBuffer source = new StringBuffer();
					source.append(selfURN.getURI());
					source.append("[");
					source.append(point.offset);
					source.append(",");
					source.append(point.length);
					source.append("]");
					if (s == null && p == null) {
						res.add(new Quad(Node.ANY,selfURN, AFF4.contains, Node.createURI(source.toString())));
						res.add(new Quad(Node.ANY,Node.createURI(source.toString()), AFF4.sameAs, Node.createURI(target.toString())));
					} else if (s.equals(source.toString())) {
						res.add(new Quad(Node.ANY,Node.createURI(source.toString()), AFF4.sameAs, Node.createURI(target.toString())));
					} else if (s.equals(selfURN) && p.equals(AFF4.contains)) {
						res.add(new Quad(Node.ANY,selfURN, AFF4.contains, Node.createURI(source.toString())));
					}
				}
			}
			
		}
		return res;
	}
	
	protected void doLine(String line) {
		int firstComma = line.indexOf(',');
		int secondComma = line.indexOf(',',firstComma+1);
		String offsetStr = line.substring(0,firstComma);
		String targetOffsetStr = line.substring(firstComma+1,secondComma);
		long offset = Long.parseLong(offsetStr);
		long targetOffset = Long.parseLong(targetOffsetStr);
		String targetURN = line.substring(secondComma+1);
		
		if (firstLine) {
			currentPoint = new Point(offset, 0, targetOffset, Node.createURI(targetURN));
			firstLine = false;
		} else {
			currentPoint.length = offset - currentPoint.offset;
			result.add(currentPoint);
			currentPoint = new Point(offset, 0, targetOffset, Node.createURI(targetURN));
		}
	}
	
	protected void finish() {
		currentPoint.length = streamSize - currentPoint.offset;
		result.add(currentPoint);
		currentPoint = null;
	}
	
	public String toString() {
		StringBuffer sb = new StringBuffer();
		for (Point p : result) {
			sb.append(selfURN);
			sb.append("[");
			sb.append(p.offset);
			sb.append(",");
			sb.append(p.length);
			sb.append("]=");
			sb.append(p.targetURN);
			sb.append("[");
			sb.append(p.offset);
			sb.append(",");
			sb.append(p.length);
			sb.append("]\r\n");
		}
		return sb.toString();
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