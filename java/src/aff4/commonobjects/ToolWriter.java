package aff4.commonobjects;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.net.URLEncoder;
import java.util.ArrayList;

import aff4.infomodel.serialization.PropertiesWriter;
import aff4.storage.zip.WritableZipVolume;

public class ToolWriter extends Instance {
	ArrayList<String> assertions = new ArrayList<String>();
	String toolName;
	String toolURL;
	String toolVersion;
	
	public ToolWriter(WritableZipVolume v) {
		super(v);
		volume = v;
	}

	public void addAssertion(String graph) {
		assertions.add(graph);
	}
	
	
	public String getToolName() {
		return toolName;
	}

	public void setToolName(String toolName) {
		this.toolName = toolName;
	}

	public String getToolURL() {
		return toolURL;
	}

	public void setToolURL(String toolURL) {
		this.toolURL = toolURL;
	}

	public String getToolVersion() {
		return toolVersion;
	}

	public void setToolVersion(String toolVersion) {
		this.toolVersion = toolVersion;
	}

	public void close() throws FileNotFoundException, IOException{
		String URN = getURN();
		for (String urn : assertions) {
			volume.add(URN + "/properties", urn, "aff4:assertedBy", URN );

		}

		volume.add(URN + "/properties", URN , "aff4:type", "aff4:Tool");
		volume.add(URN + "/properties", URN + "/properties", "aff4:assertedBy", URN );
		volume.add(URN + "/properties", URN, "aff4:name", toolName);
		volume.add(URN + "/properties", URN, "aff4:version", toolVersion);
		volume.add(URN + "/properties", URN, "aff4:toolURL", toolURL);

		String name = URLEncoder.encode(URN, "UTF-8")	+ "/properties";
		
		PropertiesWriter writer = new PropertiesWriter(URN);
		String res = writer
					.write(volume.query(URN + "/properties", null, null, null));
		OutputStream f = volume.createOutputStream(name, true, res.length());
		f.write(res.getBytes());
		f.close();

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
