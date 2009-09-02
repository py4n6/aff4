package aff4.container;

import java.io.IOException;
import java.text.ParseException;

import aff4.datamodel.Reader;
import aff4.datamodel.Writer;
import aff4.infomodel.Node;
import aff4.infomodel.QuadList;
import aff4.infomodel.Resource;
import aff4.storage.zip.AFF4SegmentOutputStream;
import aff4.storage.zip.StreamWriter;
import aff4.storage.zip.WritableZipVolume;

public class Container implements ReadWriteContainer {
	String writeContainerPath;
	DirectoryCorpus corpus = null;
	WritableZipVolume outputVolume;
	
	public Container(String directoryPath, String writeContainterPath) throws IOException {
		this.writeContainerPath = writeContainterPath;
		if (directoryPath != null)
			corpus = new DirectoryCorpus(directoryPath);
		outputVolume = new WritableZipVolume(writeContainterPath);
		
	}
	
	public Reader open(Resource segment) throws IOException, ParseException {
		if (corpus == null) 
			return null;
		return corpus.open(segment);
	}
	
	public void close() throws IOException {
		outputVolume.close();
		if (corpus != null)
			corpus.close();
	}
	public QuadList query(Node g, Node s, Node p, Node o)
			throws IOException, ParseException {
		QuadList result = new QuadList();
		if (corpus != null)
			result.addAll(corpus.query(g, s, p, o));
		result.addAll(outputVolume.query(g, s, p, o));
		return result;
	}
	
	public AFF4SegmentOutputStream createOutputStream(String name,
			boolean compressed, long uncompresedSize) throws IOException {
		return outputVolume.createOutputStream(name, compressed, uncompresedSize);
	}
	
	public StreamWriter createWriter() throws IOException {
		return outputVolume.createWriter();
	}
	
	public void add(Node graph, Node subject, Node property, Node object) {
		outputVolume.add(graph, subject, property, object);
	}
	
	public Resource getURN() {
		return outputVolume.getURN();
	}

	public Resource getGraph() {
		return outputVolume.getGraph();
	}
}
