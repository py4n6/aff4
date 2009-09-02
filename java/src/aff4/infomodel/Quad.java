package aff4.infomodel;

public class Quad implements Comparable<Quad> {
	Resource graph = null;
	Resource subject = null;
	Resource predicate = null;
	Node object = null;
	
	public Resource getGraph() {
		return graph;
	}

	public void setGraph(Resource graph) {
		this.graph = graph;
	}

	public Resource getSubject() {
		return subject;
	}

	public void setSubject(Resource subject) {
		this.subject = subject;
	}

	public Resource getPredicate() {
		return predicate;
	}

	public void setPredicate(Resource predicate) {
		this.predicate = predicate;
	}

	public void setPredicate(String predicate) {
		this.predicate = (Resource) Node.createURI(predicate);
	}
	
	public Node getObject() {
		return object;
	}


	
	public Quad(String g, String s, String p, String o) {
		this.graph = (Resource) Node.createURI(g);
		this.subject = (Resource) Node.createURI(s);
		this.predicate = (Resource) Node.createURI(p);
		this.object = Node.createLiteral(o, null, null);
	}
	
	public Quad(Resource g, Resource s, Resource p, Node o) {
		graph = g;
		subject = s;
		predicate = p;
		object = o;
	}
	
	public Quad(Node g, Node s, Node p, Node o) {
		graph = (Resource)g;
		subject = (Resource)s;
		predicate = (Resource)p;
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

	public int compareTo(Quad o) {
		if (o == null) {
			throw new NullPointerException();
		}
		
		return toString().compareTo(o.toString());
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