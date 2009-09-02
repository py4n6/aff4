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

import aff4.commonobjects.LinkWriter;
import aff4.infomodel.Node;
import aff4.infomodel.QuadList;
import aff4.infomodel.lexicon.AFF4;
import aff4.storage.zip.ReadOnlyZipVolume;
import aff4.storage.zip.StreamReader;
import aff4.storage.zip.StreamWriter;
import aff4.storage.zip.WritableZipVolume;

public class aff4dump {

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
		
		Option linkOption = OptionBuilder.withArgName( "link" )
	        .hasArg()
	        .withDescription(  "link name" )
	        .withLongOpt( "link" )
	        .create("l");
		options.addOption(linkOption);
		
		
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
			
			if (line.hasOption("o") && line.getArgs().length == 1) {
				String outputFile = line.getOptionValue("o");
				String inputFile = line.getArgs()[0];
				if (outputFile != null && inputFile != null) {
					File in = new File(inputFile);
					ReadOnlyZipVolume rzv = new ReadOnlyZipVolume(inputFile);
					RandomAccessFile raf = new RandomAccessFile(outputFile, "rw");
					
					QuadList res = rzv.query(Node.ANY, Node.ANY, AFF4.type, AFF4.image);
					
					StreamReader reader = new StreamReader(rzv, res.get(0).getSubject());
					
					ByteBuffer buf = ByteBuffer.allocate(64*1024);
					int bytesRead;
					while ((bytesRead = reader.read(buf)) != -1) {
						raf.write(buf.array(), 0, buf.limit());
					}
					
					raf.close();
					reader.close();
					rzv.close();
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
