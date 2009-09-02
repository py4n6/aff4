package aff4.infomodel.serialization;

import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.Resource;

public class PropertiesWriter {

	Resource URN;
	public PropertiesWriter(Resource URN) {
		this.URN = URN;
	}
	
	public String write(QuadList quads) {
		StringBuffer buf = new StringBuffer();
		for (Quad q : quads) {
			if (q.getSubject().equals(URN)) {
				buf.append(q.getPredicate());
				buf.append("=");
				buf.append(q.getObject());
				buf.append("\r\n");
			} else {
				buf.append(q.getSubject());
				buf.append(" ");
				buf.append(q.getPredicate());
				buf.append(" ");
				buf.append(q.getObject());
				buf.append("\r\n");
			}
		}
		return buf.toString();
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