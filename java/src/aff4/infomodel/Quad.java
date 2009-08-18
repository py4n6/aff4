package aff4.infomodel;

public class Quad {
	String graph = null;
	public String getGraph() {
		return graph;
	}

	public void setGraph(String graph) {
		this.graph = graph;
	}

	public String getSubject() {
		return subject;
	}

	public void setSubject(String subject) {
		this.subject = subject;
	}

	public String getPredicate() {
		return predicate;
	}

	public void setPredicate(String predicate) {
		this.predicate = predicate;
	}

	public String getObject() {
		return object;
	}

	public void setObject(String object) {
		this.object = object;
	}

	String subject = null;
	String predicate = null;
	String object = null;
	
	public Quad(String g, String s, String p, String o) {
		graph = g;
		subject = s;
		predicate = p;
		object = o;
	}
	
	public String toString() {
		StringBuffer sb = new StringBuffer();

			sb.append(graph);
			sb.append(", ");
			sb.append(subject);
			sb.append(", ");
			sb.append(predicate);
			sb.append(", ");
			sb.append(object);


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