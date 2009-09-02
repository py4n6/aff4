package aff4.storage.zip;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.StringWriter;
import java.util.UUID;

import aff4.container.WritableStore;
import aff4.datamodel.Writable;
import aff4.datamodel.Writer;
import aff4.infomodel.Literal;
import aff4.infomodel.NamedGraphSet;
import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QuadStore;
import aff4.infomodel.Queryable;
import aff4.infomodel.Resource;
import aff4.infomodel.Storable;
import aff4.infomodel.aff4il.AFF4ILWriter;
import aff4.infomodel.lexicon.AFF4;
import aff4.infomodel.serialization.PropertiesWriter;
import de.schlichtherle.util.zip.ZipEntry;
import de.schlichtherle.util.zip.ZipOutputStream;

public class WritableZipVolume implements WritableStore {
	String path;
	Resource volumeURN;
	Resource graphURN;
	ZipOutputStream zipfile = null;
	String archiveName = null;
	QuadStore quadStore = null;
	FileOutputStream zipStream = null;
	
	public WritableZipVolume(String p) throws IOException {
		path = p;
		int lastSlash = p.lastIndexOf(File.separator);
		archiveName = p.substring(lastSlash + 1);
		volumeURN = Node.createURI("urn:aff4:" + UUID.randomUUID().toString());
		zipStream = new FileOutputStream(path);
		zipfile = new ZipOutputStream(zipStream);
		quadStore = new QuadStore();
		
		add(getGraph(), volumeURN, AFF4.type, AFF4.zip_volume);
		add(getGraph(), volumeURN, AFF4.iface, AFF4.volume);
		add(getGraph(), volumeURN, AFF4.stored, new Literal(archiveName));
	}
	
	public Resource getURN() {
		if (volumeURN == null) {
			volumeURN = Node.createURI("urn:aff4:" + UUID.randomUUID().toString());
		}
		return volumeURN;
	}
	
	public Resource getGraph() {
		if (graphURN == null) {
			graphURN = Node.createURI("urn:aff4:" + UUID.randomUUID().toString());
		}
		return graphURN;
	}
	
	public void close() throws IOException {
		zipfile.setComment(getURN().getURI());
		//ZipFile zf = new ZipFile(path);

		
		String name =  "info";
		ZipEntry ze = new ZipEntry(name);
		zipfile.putNextEntry(ze);
		
		StringWriter sw = new StringWriter();
		AFF4ILWriter tw = new AFF4ILWriter();
		tw.addNamespace("aff4", AFF4.baseURI);
		tw.write(quadStore, sw, "http://foo.net/");
		sw.toString();
		

		zipfile.write(sw.toString().getBytes());
		zipfile.closeEntry();
		zipfile.finish();
		zipfile.close();
		zipStream.close();
		
		//AppendableZipFile azs = new AppendableZipFile(path);
		//azs.setComment(volumeURN);
		//azs.close();
	}
	
	public void add(Node graph, Node subject, Node property, Node object) {
		quadStore.add(new Quad(graph, subject, property, object));
	}
	
	public QuadList query(Node g, Node s, Node p, Node o) throws IOException {
		return quadStore.query(g, s, p, o);
	}
	
	public String getPath() {
		return path;
	}
	
	public QuadStore getQuadStore() {
		return quadStore;
	}
	
	public AFF4SegmentOutputStream createOutputStream(String name, boolean compressed, long uncompresedSize) throws IOException {
		ZipEntry ze = new ZipEntry(name);
		if (compressed) {
			ze.setMethod(ZipEntry.DEFLATED);
			zipfile.setMethod(ZipEntry.DEFLATED);
			
		} else {
			ze.setMethod(ZipEntry.STORED);
			zipfile.setMethod(ZipEntry.STORED);
		}
		
		
		return new AFF4SegmentOutputStream(ze,zipfile, uncompresedSize);
		
	}
	
	public StreamWriter createWriter() {
		return open();
	}
	public StreamWriter open() {
		StreamWriter sw = new StreamWriter(this, zipfile);
		return sw;
	}
	
	public ZipOutputStream getZipFile() {
		return zipfile;
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
