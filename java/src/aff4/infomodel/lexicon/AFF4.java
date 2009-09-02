package aff4.infomodel.lexicon;

import aff4.infomodel.Resource;

public class AFF4 {
	public static String baseURI = "http://aff.org/2009/aff4#";
	public static Resource signature = new Resource(baseURI +  "signature");
	public static Resource hash = new Resource(baseURI +  "hash");
	public static Resource chunk_size = new Resource(baseURI +  "chunk_size");
	public static Resource chunks_in_segment = new Resource(baseURI +  "chunks_in_segment");
	public static Resource compression = new Resource(baseURI +  "compression");
	public static Resource size = new Resource(baseURI +  "size");
	public static Resource type = new Resource(baseURI +  "type");
	public static Resource zip_volume = new Resource(baseURI +  "zip_volume");
	public static Resource iface = new Resource(baseURI +  "interface");
	public static Resource stored = new Resource(baseURI +  "stored");
	public static Resource volume = new Resource(baseURI +  "volume");
	public static Resource image = new Resource(baseURI +  "image");
	public static Resource stream = new Resource(baseURI +  "stream");
	public static Resource contains = new Resource(baseURI +  "contains");
	public static Resource target = new Resource(baseURI +  "target");
	public static Resource assertedBy = new Resource(baseURI +  "assertedBy");
	public static Resource name = new Resource(baseURI +  "name");
	public static Resource version = new Resource(baseURI +  "version");
	public static Resource toolURI = new Resource(baseURI +  "toolURI");
	public static Resource Tool = new Resource(baseURI +  "Tool");
	public static Resource sha256 = new Resource(baseURI +  "sha256");
	public static Resource md5 = new Resource(baseURI +  "md5");
	public static Resource publicKeyCertificate = new Resource(baseURI +  "publicKeyCertificate");
	public static Resource identity = new Resource(baseURI +  "identity");
	public static Resource authority = new Resource(baseURI +  "authority");
	public static Resource digestMethod = new Resource(baseURI +  "digestMethod");
	public static Resource signatureMethod = new Resource(baseURI +  "signatureMethod");
	public static Resource Warrant = new Resource(baseURI +  "Warrant");
	public static Resource sameAs = new Resource(baseURI +  "sameAs");
	public static Resource map = new Resource(baseURI +  "map");
	public static Resource containsSegmentIndex = new Resource(baseURI +  "containsSegmentIndex");
	public static Resource containsSegment = new Resource(baseURI +  "containsSegment");
	public static Resource BevvyIndex = new Resource(baseURI +  "BevvyIndex");
	public static Resource BevvySegment = new Resource(baseURI +  "BevvySegment");
	public static Resource trans = new Resource(baseURI +  "transient");
}
