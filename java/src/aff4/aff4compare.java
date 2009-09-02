package aff4;

import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;

import aff4.infomodel.Node;
import aff4.infomodel.QuadList;
import aff4.infomodel.lexicon.AFF4;
import aff4.storage.zip.ReadOnlyZipVolume;
import aff4.storage.zip.StreamReader;

public class aff4compare {

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		
		Options options = new Options();

	
		
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
				formatter.printHelp( "aff4imager", options );
			    System.exit(0);
			}
			
				String volume = line.getArgs()[0];
				String source = line.getArgs()[1];
					
				ReadOnlyZipVolume rzv = new ReadOnlyZipVolume(volume);
				FileInputStream raf = new FileInputStream(source);
				FileChannel channel = raf.getChannel();
				
				QuadList res = rzv.query(Node.ANY, Node.ANY, AFF4.type, AFF4.image);
				
				StreamReader reader = new StreamReader(rzv, res.get(0).getSubject());
				ByteBuffer sourceBuf = ByteBuffer.allocate(64*1024);
				long offset = 0;
				

				ByteBuffer buf = ByteBuffer.allocate(64*1024);
				int bytesRead = 0;
				while ((bytesRead = reader.read(buf)) != -1) {
					
					channel.read(sourceBuf);
					sourceBuf.position(0);
					int rr = sourceBuf.compareTo(buf);
					
					offset = offset + 64*1024;
					if (rr != 0) {
						sourceBuf.position(0);
						buf.position(0);
						for (int i = 0; i < sourceBuf.limit() ; i++) {
							if (sourceBuf.get() != buf.get()) {
								System.out.println("Problem at offset: " + (offset + i) );
								System.exit(0);
							}
						}
						
					}
				}
				System.out.println("Comared OK.");
				channel.close();
				raf.close();
				reader.close();
				rzv.close();


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
