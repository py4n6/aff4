package aff4.container;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import aff4.datamodel.Readable;
import aff4.datamodel.Reader;
import aff4.infomodel.Node;
import aff4.infomodel.QuadList;
import aff4.infomodel.QueryTools;
import aff4.infomodel.Queryable;
import aff4.infomodel.Resource;
import aff4.storage.zip.ReadOnlyZipVolume;

public class DirectoryCorpus implements ReadOnlyContainer {
	String directory;
	List<Readable> volumes;
	Map<Resource, Readable> graphSource = null;
	public DirectoryCorpus(String directory) {
		this.directory = directory;
		File dir = new File(directory);
		graphSource = new HashMap<Resource, Readable>();
		if (dir.isDirectory()) {
			volumes = new ArrayList<Readable>();
			File[] volumeFiles = dir.listFiles(new FileFilter() {

				public boolean accept(File pathname) {
					if (pathname.isFile() && pathname.getName().endsWith(".zip")) {
						return true;
					}
					return false;
				}
				
			});
			for (File volume : volumeFiles) {
				try {
					ReadOnlyZipVolume zv = new ReadOnlyZipVolume(volume);
					volumes.add(zv);
					QuadList res = zv.query(null, null, null, null);
					for (Resource graph : QueryTools.getUniqueGraphs(res)) {
						graphSource.put(graph, zv);
					}
					
				} catch (IOException e) {
					System.err.println(volume.getName() + " not added");
					e.printStackTrace();
				} catch (ParseException e) {
					System.err.println(volume.getName() + " not added");
					e.printStackTrace();
				}
			}
			
		}
	}

	public Reader open(Resource urn) throws IOException, ParseException {
		for (Readable v : volumes) {
			Reader r = v.open(urn);
			if (r != null)
				return r;
		}
		return null;
	}

	public void close() throws IOException {
		for (Readable v : volumes) {
			v.close();
		}

	}

	public QuadList query(Node g, Node s, Node p, Node o)
			throws IOException, ParseException {
		QuadList res = null;
		for (Readable v : volumes) {
			if (res == null) {
				res = v.query(g,s,p,o);
			} else {
				res.addAll(v.query(g,s,p,o));
			}

		}
		return res;
	}

	public Resource getURN() {
		throw new RuntimeException();
	}
	
	public Resource getGraph() {
		throw new RuntimeException();
	}
}
