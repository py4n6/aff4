package aff4.commonobjects;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.net.URLEncoder;
import java.util.ArrayList;

import aff4.infomodel.Node;
import aff4.infomodel.Resource;
import aff4.infomodel.lexicon.AFF4;
import aff4.infomodel.serialization.PropertiesWriter;
import aff4.storage.zip.WritableZipVolume;

public class ToolWriter extends Instance {
	ArrayList<Resource> assertions = new ArrayList<Resource>();
	String toolName;
	String toolURL;
	String toolVersion;
	
	public ToolWriter(WritableZipVolume v) {
		super(v);
		volume = v;
	}

	public void addAssertion(Resource graph) {
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
		Resource URN = getURN();
		for (Resource urn : assertions) {
			volume.add(volume.getGraph(), urn, AFF4.assertedBy, URN );

		}

		volume.add(volume.getGraph(), URN , AFF4.type, AFF4.Tool);
		volume.add(volume.getGraph(), volume.getGraph(), AFF4.assertedBy, URN );
		volume.add(volume.getGraph(), URN, AFF4.name, Node.createLiteral(toolName, null, null));
		volume.add(volume.getGraph(), URN, AFF4.version, Node.createLiteral(toolVersion,null,null));
		volume.add(volume.getGraph(), URN, AFF4.toolURI, Node.createURI(toolURL));

		/*
		String name = URLEncoder.encode(URN.getURI(), "UTF-8")	+ "/properties";
		
		PropertiesWriter writer = new PropertiesWriter(URN);
		String res = writer
					.write(volume.query(Node.createURI(URN + "/properties"), Node.ANY, Node.ANY, Node.ANY));
		OutputStream f = volume.createOutputStream(name, true, res.length());
		f.write(res.getBytes());
		f.close();
		*/

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
