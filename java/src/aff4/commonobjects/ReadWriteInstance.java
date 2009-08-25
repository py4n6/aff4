package aff4.commonobjects;

import aff4.container.Container;
import aff4.container.WritableStore;
import aff4.infomodel.AFFObject;
import aff4.storage.zip.WritableZipVolume;

public class ReadWriteInstance  extends AFFObject {
	protected WritableStore container = null;
	
	public ReadWriteInstance(WritableStore v) {
		container = v;
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