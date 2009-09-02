package aff4.commonobjects;

import java.io.IOException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.Signature;
import java.security.SignatureException;
import java.text.ParseException;
import java.util.ArrayList;

import org.bouncycastle.crypto.digests.SHA256Digest;
import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.bouncycastle.util.encoders.Base64;

import aff4.container.ReadOnlyContainer;
import aff4.hash.HashDigestAdapter;
import aff4.infomodel.GraphCanonicalizer;
import aff4.infomodel.Literal;
import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QueryTools;
import aff4.infomodel.Resource;
import aff4.infomodel.TooManyValuesException;
import aff4.infomodel.lexicon.AFF4;
import aff4.storage.zip.ReadOnlyZipVolume;

public class WarrantReader {
	ReadOnlyContainer volume;
	Resource URN = null;
	
	ArrayList<Resource> assertions = new ArrayList<Resource>();
	
	public WarrantReader(ReadOnlyContainer v, Resource u) {
		volume = v;
		URN = u;
	}
	
	public ArrayList<Resource> getAssertions() {
		return assertions;
	}
	
	public boolean isValid() throws IOException, NoSuchAlgorithmException, InvalidKeyException, SignatureException, TooManyValuesException, ParseException {

			Resource warrantURN = URN;
			Resource authority = (Resource) QueryTools.queryValue(volume, Node.ANY, warrantURN, AFF4.authority);

			boolean verified = false;
			
			Resource warrantGraph = volume.query(Node.ANY, warrantURN, AFF4.type, AFF4.Warrant).get(0).getGraph();
			
			QuadList signedStatements = volume.query(warrantGraph, Node.ANY, Node.ANY, Node.ANY);
			GraphCanonicalizer standardiser = new GraphCanonicalizer(signedStatements);
			String canonicalData= standardiser.getCanonicalString();

			byte[] bytes = canonicalData.getBytes("UTF-8");
			AuthorityReader authorityReader = new AuthorityReader(volume,authority);
			
			Signature signature = Signature.getInstance("SHA256withRSA", new BouncyCastleProvider());
			signature.initVerify(authorityReader.publicKey);
			signature.update(bytes);
			
			String sig = ((Literal) QueryTools.queryValue(volume, Node.ANY, warrantGraph, AFF4.signature)).asString();
			byte[] signatureBytes = Base64.decode(sig);
			if (!signature.verify(signatureBytes)) {
				return false; 
			}

			HashDigestAdapter hasher = new HashDigestAdapter(new SHA256Digest());
			QuadList graphs = volume.query( Node.ANY, Node.ANY, AFF4.assertedBy, warrantURN);
			for (Quad graph : graphs) {
				Resource subjectGraph = graph.getSubject();
				if (!subjectGraph.equals(warrantGraph)) {
					//String digestMethod = ((Literal)QueryTools.queryValue(volume, Node.ANY, subjectGraph, AFF4.digestMethod)).asString();
					String digest = ((Literal)QueryTools.queryValue(volume, Node.ANY, subjectGraph, AFF4.hash)).asString();
					
					QuadList statements = volume.query(subjectGraph, null, null, null);
					standardiser = new GraphCanonicalizer(statements);
					hasher.reset();
					hasher.update(standardiser.getCanonicalString());
					hasher.doFinal();
	
					String calculatedHash =hasher.getStringValue();
					if (!calculatedHash.equals(digest)) {
						return false;
					} else {
						assertions.add(subjectGraph);
					}
				}
			}

		return true;
		
	}
}

/*
Advanced Forensic Format v.4 http://www.afflib.org/

Copyright (C) 2009  Bradley Schatz <bradley@schatzforensic.com.au>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/