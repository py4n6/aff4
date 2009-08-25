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
import aff4.storage.zip.StreamWriter;
import aff4.storage.zip.WritableZipVolume;

public class aff4imager {

	/**
	 * @param args
	 * @throws java.text.ParseException 
	 */
	public static void main(String[] args) throws java.text.ParseException {
		
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
		
		Option chunkOption = OptionBuilder.withArgName( "chunk_in_segment" )
	        .hasArg()
	        .withDescription(  "chunks in segment" )
	        .withLongOpt( "chunks_in_segment" )
	        .create("c");
		options.addOption(chunkOption);
		
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
				formatter.printHelp( "aff4imager -o imagefile source", options );
			    System.exit(0);
			}
			
			if (line.hasOption("o") && line.getArgs().length == 1) {
				String outputFile = line.getOptionValue("o");
				String inputFile = line.getArgs()[0];
				if (outputFile != null && inputFile != null) {
					File in = new File(inputFile);
					RandomAccessFile raf = new RandomAccessFile(in, "r");
					
					WritableZipVolume zv = new WritableZipVolume(outputFile);
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
		}
	}
		
}
