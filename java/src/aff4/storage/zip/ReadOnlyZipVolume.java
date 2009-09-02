package aff4.storage.zip;

import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.StringReader;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.net.URLEncoder;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.List;

import aff4.commonobjects.StreamResolver;
import aff4.container.ReadOnlyContainer;
import aff4.datamodel.MapReader;
import aff4.datamodel.Readable;
import aff4.datamodel.Reader;
import aff4.infomodel.Literal;
import aff4.infomodel.NamedGraphSet;
import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QuadStore;
import aff4.infomodel.Queryable;
import aff4.infomodel.Resource;
import aff4.infomodel.aff4il.NamedGraphSetPopulator;
import aff4.infomodel.aff4il.parser.AFF4ILParser;
import aff4.infomodel.datatypes.DataType;
import aff4.infomodel.lexicon.AFF4;
import aff4.infomodel.serialization.MapResolver;
import aff4.infomodel.serialization.Properties2Resolver;
import aff4.infomodel.serialization.PropertiesResolver;
import antlr.RecognitionException;
import antlr.TokenStreamException;
import de.schlichtherle.util.zip.ZipEntry;
import de.schlichtherle.util.zip.ZipFile;


public class ReadOnlyZipVolume implements ReadOnlyContainer {
	Resource volumeURN = null;
	Resource graph = null;
	
	ArrayList<Resource> instances = null;
	QuadStore store = null;
	ZipFile zf = null;
	HashSet<ZipEntry> propertiesEntries = null;
	HashSet<ZipEntry> mapEntries = null;
	HashSet<ZipEntry> otherEntries = null;
	String path;

	public ReadOnlyZipVolume(String path) throws IOException {
		zf = new ZipFile(path);
		volumeURN = Node.createURI(zf.getComment());
		this.path = path;
	}
	
	public ReadOnlyZipVolume(File path) throws IOException {
		zf = new ZipFile(path);
		volumeURN = Node.createURI(zf.getComment());
		this.path = path.getName();
	}
	
	
	public QuadList query(Node g, Node s, Node p, Node o) throws IOException, ParseException {
		if (store == null) {
			// first query we cache all of the results in the quadstore
			store = new QuadStore();
			QuadList res = queryNoCache(null, null, null, null);
			store.add(res);
		}
		return store.query(g, s, p, o);
		
	}
	

	public QuadList queryNoCache(Node g, Node s, Node p, Node o) throws IOException, ParseException {
		ZipEntry e = null;
		instances = new ArrayList<Resource>();
		Enumeration<ZipEntry> it = zf.entries();
		ArrayList<ZipEntry> entries = new ArrayList<ZipEntry>();
		while (it.hasMoreElements()) {
			e = it.nextElement();
			entries.add(e);
		}
		
		// run through once to pick up all the properties segments,
		// processing the properties segments as we go
		QuadList results = new QuadList();
		propertiesEntries = new HashSet<ZipEntry>();
		mapEntries = new HashSet<ZipEntry>();
		otherEntries = new HashSet<ZipEntry>();
		for (ZipEntry ent : entries) {
			if (ent.getName().endsWith("info")) {
				List<Quad> result = queryInfoSegment(g,s,p,o,zf, ent);
				propertiesEntries.add(ent);
				results.addAll(result);
			} else if (ent.getName().endsWith("map")) {
				mapEntries.add(ent);
			} else if (ent.getName().endsWith(".idx")){
				int lastSlash = ent.getName().lastIndexOf('/');
				String instancePath = ent.getName().substring(0,lastSlash);
				Resource instance = Node.createURI(URLDecoder.decode(instancePath, "UTF-8"));
				if (!instances.contains(instance)) {
					instances.add(instance);
				}
				otherEntries.add(ent);
				
			} else {
				otherEntries.add(ent);
			}

		}

		// process any map entries
		for (ZipEntry ent : mapEntries) {
			List<Quad> result = queryMapSegment(g,s,p,o,zf, ent);
			results.addAll(result);
		}
		
		// for each image pull out metadata
		for (Resource i : instances) {
			//if (store.query(Node.ANY, i, AFF4.type, AFF4.image).size() == 1) {
				// this is an image instance
				StreamResolver sr = new StreamResolver(this, otherEntries);
				QuadList result = sr.query(Node.ANY, i, Node.ANY, Node.ANY);
				results.addAll(result);
			//}
			
		}
		
		// process any bevvies
		results.addAll(queryBevvies(g,s,p,o));
		
		return results;
	}
	
	QuadList queryBevvies(Node g, Node s, Node p, Node o) throws UnsupportedEncodingException {
		HashSet<String> indexeNames = new HashSet<String>();
		HashSet<ZipEntry> indexes = new HashSet<ZipEntry>();
		HashSet<ZipEntry> bevvySegments = new HashSet<ZipEntry>();
		
		QuadList inferredProperties = new QuadList();
		
		
		for (ZipEntry e : otherEntries) {
			if (e.getName().endsWith(".idx")) {
				// this is a bevvy index
				indexeNames.add(e.getName());
				indexes.add(e);
				inferredProperties.add(new Quad(AFF4.trans, 
						Node.createURI(URLDecoder.decode(e.getName(), "UTF-8")), 
						AFF4.size, Node.createLiteral(Long.toString(e.getSize()), null, DataType.integer)));
				inferredProperties.add(new Quad(AFF4.trans, 
						Node.createURI(URLDecoder.decode(e.getName(), "UTF-8")), 
						AFF4.type, AFF4.BevvyIndex));
			}
		}
		
		for (ZipEntry e : otherEntries) {
			if (indexeNames.contains(e.getName() + ".idx")) {
				bevvySegments.add(e);
				inferredProperties.add(new Quad(AFF4.trans, 
						Node.createURI(URLDecoder.decode(e.getName(), "UTF-8")), 
						AFF4.size, Node.createLiteral(Long.toString(e.getSize()), null, DataType.integer)));
				inferredProperties.add(new Quad(AFF4.trans, 
						Node.createURI(URLDecoder.decode(e.getName(), "UTF-8")), 
						AFF4.type, AFF4.BevvySegment));			}
		}
		
		QuadStore qs = new QuadStore(inferredProperties);
		return qs.query(g, s, p, o);
		
		
	}

	QuadList queryInfoSegment(Node g, Node s, Node p, Node o, ZipFile zf, ZipEntry e) throws IOException, ParseException {
		try {
			java.io.Reader r = new InputStreamReader(zf.getInputStream(e));
			NamedGraphSet ngs = new NamedGraphSet();
			NamedGraphSetPopulator ngsp = new NamedGraphSetPopulator(ngs, "foo", "foo");
			AFF4ILParser parser = new AFF4ILParser(r, ngsp);
			parser.parse();
			r.close();
			QuadStore qs = new QuadStore(ngs);
			QuadList res = qs.query(Node.ANY, volumeURN, AFF4.type, AFF4.zip_volume);
			if (res.size() == 1) {
				graph = res.get(0).getGraph();
			}
			return qs.query(g, s, p, o);
		} catch (TokenStreamException ex) {
			throw new ParseException(ex.getLocalizedMessage(), -1);
		} catch (RecognitionException ex2) {
			// TODO Auto-generated catch block
			throw new ParseException(ex2.getLocalizedMessage(), ex2.getLine());
		}
		
	}

	
	/*
	QuadList queryPropertiesSegment(String g, String s, String p, String o, ZipFile zf, ZipEntry e) throws IOException, ParseException {
		String path = e.getName();
		path = URLDecoder.decode(path, "UTF-8");
		String instanceURN = null;
		
		
		if (path.endsWith("/properties")) {
			if (path.startsWith("urn:aff4")) {
				// regular instance
				int pos = path.indexOf("/properties");
				instanceURN = path.substring(0,pos);
				instances.add(instanceURN);
			} else {
				// it is a symolic name
				instanceURN = volumeURN + "/" + path.substring(0,path.indexOf("/"));
			}
		} else {
			// it is the volume properties segment
			instanceURN = volumeURN;
		}
		
		PropertiesResolver pr = new Properties2Resolver(zf.getInputStream(e), instanceURN + "/properties", instanceURN);
		QuadList res = pr.query(g, s, p, o);

		return res;
	}
	*/
	QuadList queryMapSegment(Node g, Node s, Node p, Node o, ZipFile zf, ZipEntry e) throws IOException, ParseException {
		String path = e.getName();
		path = URLDecoder.decode(path, "UTF-8");
		Resource instanceURN = null;
		
		
		if (path.endsWith("/map")) {
			if (path.startsWith("urn:aff4")) {
				// regular instance
				int pos = path.indexOf("/map");
				instanceURN = Node.createURI(path.substring(0,pos));
			} 
		}
		
		// get size of map
		List<Quad> r = queryInfoSegment(g, instanceURN, AFF4.size, o, zf, zf.getEntry(URLEncoder.encode(instanceURN.getURI(), "UTF-8")+"/info"));
		long mapSize;
		
		if (r.size() == 1) {
			mapSize = ((Literal)r.get(0).getObject()).asLong();
		} else {
			throw new RuntimeException("expected size attribute in " + instanceURN);
		}

		MapResolver pr = new MapResolver(instanceURN,mapSize, zf.getInputStream(e));
		QuadList res;
		if (s == null) {
			res = pr.query(s,p, o);
			//TODO: Check the below line
		} else if (s.getURI().startsWith(instanceURN.getURI() + "[") || s.equals(instanceURN) ) {
			res = pr.query(s,p, o);
		} else {
			res = new QuadList();
		}
		
		for (Quad q : res) {
			q.setGraph(volumeURN);
			//q.setSubject(instanceURN);
		}
		return res;
	}

	String encode(String url) throws UnsupportedEncodingException {
		return URLEncoder.encode(url, "UTF-8").replaceAll("%2F", "/");
	}
	
	public Reader open(Resource urn) throws IOException, ParseException {
		
		QuadList res = query(Node.ANY, urn, AFF4.type, Node.ANY);
		ArrayList<Node> typeList = new ArrayList<Node>();
		for (Quad q : res) {
			typeList.add(q.getObject());
		}
		
		if (typeList.contains(AFF4.image)){
			return new StreamReader(this, urn);
		} else if (typeList.contains("map")){
			return new MapReader(this,urn);
		} else if (typeList.contains("aff4:BevvyIndex") || typeList.contains("aff4:BevvySegment")) {
			return new RawReader(this,urn);
		}
		
		String segmentEncodedURN = encode(urn.getURI());
		for (ZipEntry entry : otherEntries) {
			if (entry.getName().equals(segmentEncodedURN)) {
				return new RawReader(this,urn, entry.getSize());
			}
		}
		return null;
	}
	
	public Resource getURN() {
		return volumeURN;
	}
	
	public void close() throws IOException {
		zf.close();
	}
	
	public String getPath() {
		return path;
	}
	
	public ZipFile getZipFile() {
		return zf;
	}
	
	public Resource getGraph() {
		return graph;
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