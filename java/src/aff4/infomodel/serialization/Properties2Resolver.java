package aff4.infomodel.serialization;

import java.io.InputStream;
import java.text.ParseException;

import aff4.infomodel.Quad;

public class Properties2Resolver extends PropertiesResolver {
	String volumeURN = null;
	String subjectURN = null;
	public Properties2Resolver(InputStream stream, String volumeURN, String subjectURN) {
		super(stream);
		this.subjectURN = subjectURN;
		this.volumeURN = volumeURN;
		// TODO Auto-generated constructor stub
	}
	
	protected void doLine(String line) throws ParseException {
		
		try {
			int firstSpace = line.indexOf(" ");
			int secondSpace = line.indexOf(" ", firstSpace + 1);
			int equalsCharPos = line.indexOf('=');
			
			if (firstSpace > 0 && secondSpace > firstSpace) {
				String s2 = line.substring(0, firstSpace);
				String p2 = line.substring(firstSpace + 1, secondSpace);
				String o2 = line.substring(secondSpace + 1);
				
				if (g == null || g.equals(volumeURN)) {
					if (s == null || s.equals(s2)) {
						if (p == null || p.equals(p2)) {
							if (o == null || o.equals(o2)) {
								result.add(new Quad(volumeURN,s2,p2, o2));
							}
						}
					}
				}
			} else if (equalsCharPos > 0) {
				String name = line.substring(0, equalsCharPos);
				String value = line.substring(equalsCharPos+1);
				
				if (g == null || g.equals(volumeURN)) {
					if (s == null || s.equals(subjectURN)) {
						if (p == null || p.equals(name)) {
							if (o == null || o.equals(value)) {
								result.add(new Quad(volumeURN,subjectURN,name, value));
							}
						}
					}
				} 
			} else {
				throw new ParseException(line,0);
			}

		} catch (NullPointerException ex) {
			throw new ParseException(line,0);
		} catch (StringIndexOutOfBoundsException ex) {
			throw new ParseException(line,0);
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