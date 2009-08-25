package aff4.datamodel;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.text.ParseException;

public interface Reader  {
	public void position(long s) throws IOException;
	//private ByteBuffer read(long length) throws IOException, ParseException;
	public int read(ByteBuffer buf) throws IOException, ParseException;
	public long position();
	public void close() throws IOException;
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