package aff4.infomodel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class QuadStore {

	
	ArrayList<Quad> store = new ArrayList<Quad> ();
	HashMap<String, Set<Integer>> gindex = new HashMap<String, Set<Integer>>();
	HashMap<String, Set<Integer>> sindex = new HashMap<String, Set<Integer>>();
	HashMap<String, Set<Integer>> pindex = new HashMap<String, Set<Integer>>();
	HashMap<String, Set<Integer>> oindex = new HashMap<String, Set<Integer>>();
	
	public QuadStore(List<Quad> quads) {
		add(quads);

	}
	
	public QuadStore() {

	}
	
	public void add(List<Quad> quads) {
		for (Quad q : quads) {
			add(q);
		}
	}
	
	public void add(String g, String s, String p, String o) {
		add(new Quad(g,s,p,o));
	}
	
	public void add(Quad q) {
		store.add(q);
		String g = q.graph;
		String s = q.subject;
		String p = q.predicate;
		String o = q.object;
		int pos = store.size() -1;
		
		Set<Integer> gentrys = null;
		if (!gindex.containsKey(g)) {
			gentrys = new HashSet<Integer>();
			gindex.put(g, gentrys);
		} else {
			gentrys = gindex.get(g);
		}
		gentrys.add(pos);
		
		Set<Integer> sentrys = null;
		if (!sindex.containsKey(s)) {
			sentrys = new HashSet<Integer>();
			sindex.put(s, sentrys);
		} else {
			sentrys = sindex.get(s);
		}
		sentrys.add(pos);
		
		Set<Integer> pentrys = null;
		if (!pindex.containsKey(p)) {
			pentrys = new HashSet<Integer>();
			pindex.put(p, pentrys);
		} else {
			pentrys = pindex.get(p);
		}
		pentrys.add(pos);
		
		Set<Integer> oentrys = null;
		if (!oindex.containsKey(o)) {
			oentrys = new HashSet<Integer>();
			oindex.put(o, oentrys);
		} else {
			oentrys = oindex.get(o);
		}
		oentrys.add(pos);
	}
	
	public String toString() {
		StringBuffer sb = new StringBuffer();
		sb.append("***GraphIndex***");
		for (String k : this.gindex.keySet()) {
			sb.append(k);
			sb.append(":");
			sb.append(this.gindex.get(k).toString());
			sb.append("\r\n");
			
		}		
		sb.append("***SubjectIndex***");
		for (String k : this.sindex.keySet()) {
			sb.append(k);
			sb.append(":");
			sb.append(this.sindex.get(k).toString());
			sb.append("\r\n");
			
		}
		sb.append("***{PredicateIndex***");
		for (String k : this.pindex.keySet()) {
			sb.append(k);
			sb.append(":");
			sb.append(this.pindex.get(k).toString());
			sb.append("\r\n");
			
		}
		for (Quad q: store) {
			sb.append(q.toString());
			sb.append("\r\n");
		}
		return sb.toString();
	}
	
	public QuadList query(String g, String s, String p, String o) {
		QuadList res = null; 
		
		//for (String k : sindex.keySet()) {
		//	System.out.println(k);
		//}
		
		Set<Integer> gindexes = null;
		if (g == null ) {
			/*
			gindexes = new HashSet<Integer>();
			for (int i=0 ; i < store.size() ; i++) {
				gindexes.add(i);
			}
			*/
		} else if (!gindex.containsKey(g)){
			gindexes = new HashSet<Integer>();
		} else {
			gindexes = gindex.get(g);
		}
		if (gindexes != null && gindexes.size() == 0) {
			return new QuadList();
		}
		
		Set<Integer> sindexes = null;
		if (s == null ) {
			/*
			sindexes = new HashSet<Integer>();
			for (int i=0 ; i < store.size() ; i++) {
				sindexes.add(i);
			}
			*/
		} else if (!sindex.containsKey(s)){
			sindexes = new HashSet<Integer>();
		} else {
			sindexes = sindex.get(s);
		}
		if (sindexes != null && sindexes.size() == 0) {
			return new QuadList();
		}
		
		Set<Integer> pindexes = null;
		if (p == null ) {
			/*
			pindexes = new HashSet<Integer>();
			for (int i=0 ; i < store.size() ; i++) {
				pindexes.add(i);
			}
			*/
		} else if (!pindex.containsKey(p)){
			pindexes = new HashSet<Integer>();
		} else {
			pindexes = pindex.get(p);
		}
		if (pindexes != null && pindexes.size() == 0) {
			return new QuadList();
		}
		
		Set<Integer> oindexes = null;
		if (o == null ) {
			/*
			oindexes = new HashSet<Integer>();
			for (int i=0 ; i < store.size() ; i++) {
				oindexes.add(i);
			}
			*/
		} else if (!oindex.containsKey(o)){
			oindexes = new HashSet<Integer>();
		} else {
			oindexes = oindex.get(o);
		}
		if (oindexes != null && oindexes.size() == 0) {
			return new QuadList();
		}
		
		Set<Integer> cursor = new HashSet<Integer>();
		res = new QuadList();
		boolean all = false;
		
		if (gindexes == null && sindexes == null) {
			all = true;
		} else if (gindexes != null && sindexes == null) {
			cursor.addAll(gindexes);
			all = false;
		} else if  (gindexes  == null && sindexes != null) {
			cursor.addAll(sindexes);
			all = false;
		} else {
			cursor.addAll(gindexes);
			cursor.retainAll(sindexes);
			all = false;
		}
		
		if (all && pindexes == null) {
			//
		} else if (!all && pindexes == null) {
			//
		} else if (all && pindexes != null) {
			cursor.addAll(pindexes);
			all = false;
		} else if (!all && pindexes != null) {
			cursor.retainAll(pindexes);
		}
			
		if (all & oindexes == null) {
			//
		} else if (!all && oindexes == null) {
			//
		} else if (all && oindexes != null) {
			cursor.addAll(oindexes);
			all = false;
		} else if (!all && oindexes != null) {
			cursor.retainAll(oindexes);
		}
		
		if (all) {
			for (Quad q : store) {
				res.add(q);
			}
			return res;
		}
		
		for (int i : cursor) {
			res.add(store.get(i));
		}
		return res;
		
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