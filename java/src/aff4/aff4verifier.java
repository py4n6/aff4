package aff4;

import java.io.IOException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.SignatureException;
import java.util.List;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;
import org.bouncycastle.crypto.Digest;
import org.bouncycastle.crypto.digests.MD5Digest;
import org.bouncycastle.crypto.digests.SHA256Digest;

import aff4.commonobjects.WarrantReader;
import aff4.container.DirectoryCorpus;
import aff4.infomodel.Literal;
import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QueryTools;
import aff4.infomodel.Resource;
import aff4.infomodel.SliceResource;
import aff4.infomodel.TooManyValuesException;
import aff4.infomodel.datatypes.AFF4Datatype;
import aff4.infomodel.datatypes.DataType;
import aff4.infomodel.lexicon.AFF4;
import aff4.stream.HashStreamVerifier;
import aff4.stream.DataObjectVisitOrchestrator;

public class aff4verifier {

	static boolean isDataObject(DirectoryCorpus v, Resource subject) throws IOException, java.text.ParseException {
		if (subject instanceof SliceResource) {
			return true;
		}
		
		
		for (Quad t : v.query(Node.ANY,subject, AFF4.type, Node.ANY)) {
			Node type = t.getObject();
			if (type.equals(AFF4.image) || type.equals(AFF4.map)) {
				return true;
			}
		}
		return false;
	}
	/**
	 * @param args
	 * @throws TooManyValuesException 
	 * @throws SignatureException 
	 * @throws NoSuchAlgorithmException 
	 * @throws InvalidKeyException 
	 */
	public static void main(String[] args) throws InvalidKeyException, NoSuchAlgorithmException, SignatureException, TooManyValuesException {
		
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
				formatter.printHelp( "aff4hasher", options );
			    System.exit(0);
			}
			
			if (line.getArgs().length == 1) {

				String inputDir = line.getArgs()[0];
				if (inputDir != null ) {
					
					DirectoryCorpus v = new DirectoryCorpus(inputDir);
					
					QuadList quads = v.query(Node.ANY, Node.ANY, AFF4.type, AFF4.Warrant);
					for (Quad q : quads ) {
						WarrantReader wr = new WarrantReader(v, q.getSubject());
						System.out.println("Warrant <"+q.getSubject()+"> found... ");
						if (wr.isValid()) {
							for (Resource assertion : wr.getAssertions()) {
								System.out.println("\tAssertion <" + assertion + "> verified.");
							}
							System.out.println("\tWarrant signature verified.");
						} else {
							System.out.println("Signature verify failed.");
						}
					}
					
					List<Resource> dataObjects = QueryTools.getUniqueSubjects(v.query(Node.ANY, Node.ANY, AFF4.hash, Node.ANY));
					
					DataObjectVisitOrchestrator orchestrator = new DataObjectVisitOrchestrator(v);
					QuadList res = v.query(Node.ANY, Node.ANY, AFF4.hash, Node.ANY);
					for (Quad q : res) {
						if (isDataObject(v, q.getSubject())) {
						 	Digest digest = null;
							DataType dataType = ((Literal)q.getObject()).getDatatype();
							if (dataType.equals(AFF4Datatype.md5)) {
								digest = new MD5Digest();
							} else if (dataType.equals(AFF4Datatype.sha256)) {
								digest = new SHA256Digest();
							}
							orchestrator.add(q.getSubject(), new HashStreamVerifier(digest, q.getSubject(), ((Literal)q.getObject()).asString()));
						}
					}

					
					orchestrator.run();
					

					v.close();
				}
			} else {
				HelpFormatter formatter = new HelpFormatter();
				formatter.printHelp( "aff4verifier containers_dir", options );
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
