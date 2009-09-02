package aff4;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.SignatureException;
import java.util.List;
import java.util.Set;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.OptionBuilder;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;

import aff4.commonobjects.AuthorityWriter;
import aff4.commonobjects.LinkWriter;
import aff4.commonobjects.WarrantWriter;
import aff4.container.Container;
import aff4.container.DirectoryCorpus;
import aff4.container.WritableStore;
import aff4.infomodel.Node;
import aff4.infomodel.QuadList;
import aff4.infomodel.QueryTools;
import aff4.infomodel.Resource;
import aff4.infomodel.lexicon.AFF4;
import aff4.storage.zip.StreamWriter;
import aff4.storage.zip.WritableZipVolume;

public class aff4signer {

	/**
	 * @param args
	 * @throws java.text.ParseException 
	 * @throws SignatureException 
	 * @throws NoSuchAlgorithmException 
	 * @throws InvalidKeyException 
	 */
	public static void main(String[] args) throws java.text.ParseException, InvalidKeyException, NoSuchAlgorithmException, SignatureException {
		
		Options options = new Options();

	
		Option outputOption = OptionBuilder.withArgName( "outputfile" )
            .hasArg()
            .withDescription(  "output file name" )
            .withLongOpt( "output" )
            .create("o");
		options.addOption(outputOption);
		
		Option publicKeyOption = OptionBuilder.withArgName( "public-key" )
	        .hasArg()
	        .withDescription(  "public key file in pem format" )
	        .withLongOpt( "public-key" )
	        .create("p");
		options.addOption(publicKeyOption);
		
		Option privateKeyOption = OptionBuilder.withArgName( "private-key" )
	        .hasArg()
	        .withDescription(  "private key file in pem format" )
	        .withLongOpt( "private-key" )
	        .create("k");
		options.addOption(privateKeyOption);
		
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
				String dir = line.getArgs()[0];
				if (outputFile != null && dir != null && line.hasOption("k") && line.hasOption("p")) {
					WritableStore v = new Container(dir, outputFile);
				
					AuthorityWriter a = new AuthorityWriter(v, "bradley@schatzforensic.com.au");

					a.setPrivateCert(line.getOptionValue("k"));
					a.setPublicCert(line.getOptionValue("p"));
					a.close();
				
					WarrantWriter w = new WarrantWriter(v);
					w.setAuthority(a);
					
					Set<Resource> uniqueGraphs = QueryTools.getUniqueGraphs(v.query(Node.ANY, Node.ANY, Node.ANY, Node.ANY));
					for (Resource graph : uniqueGraphs) {
						if (!graph.equals(AFF4.trans)) {
							w.addAssertion(graph);
						}
					}
					w.close();
						

					v.close();
				}
			} else {
				HelpFormatter formatter = new HelpFormatter();
				formatter.printHelp( "aff4signer -o output_container.zip -k privkey.pem -p pubkey.pem containers_dir", options );
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
