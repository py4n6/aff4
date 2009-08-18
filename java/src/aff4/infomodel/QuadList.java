package aff4.infomodel;

import java.util.ArrayList;
import java.util.List;

public class QuadList extends ArrayList<Quad> {

	private static final long serialVersionUID = -4202270932802128048L;

	public List<String> getGraphs() {
		ArrayList<String> res = new ArrayList<String>();
		for (Quad q : this) {
			res.add(q.graph);
		}
		return res;
	}
	
	public List<String> getSubjects() {
		ArrayList<String> res = new ArrayList<String>();
		for (Quad q : this) {
			res.add(q.subject);
		}
		return res;
	}
	
	public List<String> getPredicates() {
		ArrayList<String> res = new ArrayList<String>();
		for (Quad q : this) {
			res.add(q.predicate);
		}
		return res;
	}
	
	public List<String> getObjects() {
		ArrayList<String> res = new ArrayList<String>();
		for (Quad q : this) {
			res.add(q.object);
		}
		return res;
	}
	
	public String toString() {
		StringBuffer sb = new StringBuffer();
		for (Quad q : this) {
			sb.append(q.toString());
			sb.append("\r\n");
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