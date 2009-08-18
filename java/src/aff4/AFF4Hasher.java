package aff4;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.OptionBuilder;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;
import org.bouncycastle.crypto.digests.MD5Digest;
import org.bouncycastle.util.encoders.Hex;

import aff4.container.LinkWriter;
import aff4.container.WarrantReader;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.storage.ReadOnlyZipVolume;
import aff4.storage.Reader;
import aff4.storage.StreamWriter;
import aff4.storage.WritableZipVolumeImpl;

public class AFF4Hasher {

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		
		Options options = new Options();

	
		Option outputOption = OptionBuilder.withArgName( "outputfile" )
            .hasArg()
            .withDescription(  "output file name" )
            .withLongOpt( "output" )
            .create("o");
		options.addOption(outputOption);
		
		Option help = new Option( "h", "help", false,"print this message" );
		options.addOption(help);
		
		Option version = new Option( "v", "version", false,"print the version information and exit" );
		options.addOption(version);
		
		CommandLineParser parser = new PosixParser();
		try{
			CommandLine line = parser.parse( options, args);
			
			// has the buildfile argument been passed?
			if( line.hasOption( "help" ) ) {
				HelpFormatter formatter = new HelpFormatter();
				formatter.printHelp( "aff4hasher", options );
			    System.exit(0);
			}
			
			if (line.hasOption("o") && line.getArgs().length == 1) {
				String outputFile = line.getOptionValue("o");
				String inputFile = line.getArgs()[0];
				if (outputFile != null && inputFile != null) {
					ReadOnlyZipVolume v = new ReadOnlyZipVolume(inputFile);
					QuadList res = v.query(null, null, "aff4:type", "image");
					for (Quad q : res ) {
						Reader r = v.open(q.getSubject());
						String storedMD5 = v.queryValue(q.getGraph(), q.getSubject(), "aff4:hash");
						MD5Digest md5 = new MD5Digest();
						ByteBuffer buf = null;
						while ((buf = r.read(64*1024)) != null) {
							md5.update(buf.array(), buf.position(), buf.limit());
						}
						byte[] md5bytes =  new byte[md5.getDigestSize()];
						md5.doFinal(md5bytes,0);
						String calculatedMD5  = new String(Hex.encode(md5bytes),"UTF-8");
						if (storedMD5.equals(calculatedMD5)) {
							System.out.println("MD5 verified: " + storedMD5);
						} else {
							System.out.println("MD5 verifification faild");
						}
					}
					v.close();
					
					/*
					WritableZipVolumeImpl zv = new WritableZipVolumeImpl(outputFile);
					StreamWriter fd = new StreamWriter(zv, zv.getZipFile());
					
					byte[] buf = new byte[64*1024];
					long readPtr = 0;
					
					
					while (readPtr < in.length()) {
						long res = raf.read(buf);
						fd.write(ByteBuffer.wrap(buf,0,(int)res));
						readPtr = readPtr + res;
					}
					
					fd.flush();
					fd.close();
					
					if (line.hasOption("link")) {
						LinkWriter l = new LinkWriter(zv, fd, line.getOptionValue("link"));
						l.close();			
					} else {
						LinkWriter l = new LinkWriter(zv, fd, "default");
						l.close();	
					}
					zv.close();
					raf.close();
					*/
				}
			} else {
				HelpFormatter formatter = new HelpFormatter();
				formatter.printHelp( "aff4imager", options );
			    System.exit(0);
			}
		} catch (IOException ex) {
			ex.printStackTrace();
		} catch (ParseException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (java.text.ParseException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
		
}
