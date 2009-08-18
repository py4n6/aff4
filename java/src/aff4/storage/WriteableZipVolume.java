package aff4.storage;

import java.io.IOException;

public interface WriteableZipVolume extends ZipVolume {
	public void add(String graph, String subject, String property, String object) ;
	public StreamWriter open();
}
