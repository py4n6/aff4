package aff4.infomodel.serialization;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

public class MultiHashSet extends HashMap<String, Set<String>> {

	/**
	 * 
	 */
	private static final long serialVersionUID = -959645966503583053L;

	public MultiHashSet() {
		super();
	}
	
	public void put(String name, String value) {
		if (!super.containsKey(name)) {
			super.put(name, new HashSet<String>());
		}
		super.get(name).add(value);
	}
	
	public Set<String> get(String name) {
		if (!super.containsKey(name)) {
			super.put(name, new HashSet<String>());
		}
		return super.get(name);
	}
	
	public int size() {
		int count = 0;
		for (String n : super.keySet()) {
			Set<String> values = super.get(n);
			count = count +  values.size();
		}
		return count;
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
