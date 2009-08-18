package aff4.scratch;
import de.schlichtherle.crypto.SeekableBlockCipher;
import de.schlichtherle.io.*;
import de.schlichtherle.io.File;
import de.schlichtherle.io.FileInputStream;
import de.schlichtherle.io.FileOutputStream;
import de.schlichtherle.io.archive.zip.Zip32OutputArchive;
import de.schlichtherle.io.util.LEDataOutputStream;
import de.schlichtherle.util.zip.ZipEntry;
import de.schlichtherle.util.zip.ZipFile;
import de.schlichtherle.util.zip.ZipOutputStream;

import java.io.*;
import java.util.ArrayList;
import java.util.Enumeration;

import junit.framework.TestCase;



public class TestTrueZip extends TestCase{

	public void testOutPutStream() {
		try {
			java.io.RandomAccessFile f = new java.io.RandomAccessFile("C:\\Users\\bradley\\AppData\\Local\\Temp\\testZip.zip", "rw");
			SeekableOutputStream bos = new SeekableOutputStream(f);

			ZipOutputStream zos = new ZipOutputStream(bos);
			zos.setComment("test");
			ZipEntry zeEntry = new ZipEntry("test");
			zos.putNextEntry(zeEntry, true);
			zos.write(new String("Content of the first file").getBytes());
			zos.closeEntry();
			zos.close();
		} catch (IOException e) {
			fail();
		}
	}
	
	public void testWriter() {
		
		
		try {
			ZipFile zf = new ZipFile("C:\\Users\\bradley\\AppData\\Local\\Temp\\testWrite.zip");
			Enumeration<ZipEntry> e = zf.entries();
			ArrayList<ZipEntry> entries = new ArrayList<ZipEntry>();
			while (e.hasMoreElements()) {
				entries.add(e.nextElement());
			}
			java.io.RandomAccessFile f = new java.io.RandomAccessFile("C:\\Users\\bradley\\AppData\\Local\\Temp\\testWrite.zip", "rw");
			SeekableOutputStream bos = new SeekableOutputStream(f);
			f.seek(zf.getCDoffset());
			LEDataOutputStream ds = new LEDataOutputStream(bos, (int)zf.getCDoffset());
			AppendableZipOutputStream zos = new AppendableZipOutputStream(ds, entries);
			zos.setComment("foobar");
			zos.flush();
			zos.close();
		} catch (NullPointerException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (FileNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		

	}
	
	

}
