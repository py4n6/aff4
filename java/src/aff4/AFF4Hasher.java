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
import org.bouncycastle.crypto.Digest;
import org.bouncycastle.crypto.digests.MD5Digest;
import org.bouncycastle.crypto.digests.SHA256Digest;
import org.bouncycastle.util.encoders.Hex;

import aff4.commonobjects.LinkWriter;
import aff4.commonobjects.ToolWriter;
import aff4.commonobjects.WarrantReader;
import aff4.datamodel.Reader;
import aff4.hash.HashDigestAdapter;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QueryTools;
import aff4.storage.zip.ReadOnlyZipVolume;
import aff4.storage.zip.StreamWriter;
import aff4.storage.zip.WritableZipVolume;

public class aff4hasher {

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
	
		Option chunkSizeOption = OptionBuilder.withArgName( "chunksize" )
        .hasArg()
        .withDescription(  "chunk size" )
        .withLongOpt( "chunk-size" )
        .create("c");
		options.addOption(chunkSizeOption);
	
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
					long chunkSize = 64*1024; 
					if (line.hasOption("c")) {
						chunkSize = Long.parseLong(line.getOptionValue("c"));
					}
					
					ReadOnlyZipVolume v = new ReadOnlyZipVolume(inputFile);
					WritableZipVolume zv = new WritableZipVolume(outputFile);
					
					ToolWriter tr = new ToolWriter(zv);
					
					QuadList res = v.query(null, null, "aff4:type", "image");
					for (Quad q : res ) {
						long readPtr = 0;
						Reader r = v.open(q.getSubject());
						String storedMD5 = QueryTools.queryValue(v, q.getGraph(), q.getSubject(), "aff4:hash");
						HashDigestAdapter md5 = new HashDigestAdapter(new MD5Digest());
						HashDigestAdapter sha256 = new HashDigestAdapter(new SHA256Digest());
						ByteBuffer buf = ByteBuffer.allocate(64*1024);
						int readCount;
						while ((readCount = r.read(buf)) != -1) {
							md5.update(buf);
							sha256.reset();
							sha256.update(buf);
							sha256.doFinal();
							String calculatedSHA256  = sha256.getStringValue();
							zv.add(zv.getURN() + "/properties", q.getSubject() + "[" + readPtr + ":" + (buf.limit() - buf.position()) + "]" , "aff4:sha256hash", calculatedSHA256);
							readPtr += buf.limit() - buf.position();
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
					tr.addAssertion(zv.getURN() + "/properties");
					tr.setToolName("aff4hasher");
					tr.setToolURL("http://aff.org/java/aff4hasher");
					tr.setToolVersion("0.1");
					tr.close();
					zv.close();
					v.close();
				}
			} else {
				HelpFormatter formatter = new HelpFormatter();
				formatter.printHelp( "aff4hasher -o container2.zip image.zip", options );
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
