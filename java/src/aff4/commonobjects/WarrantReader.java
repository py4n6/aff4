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
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QueryTools;
import aff4.infomodel.TooManyValuesException;
import aff4.storage.zip.ReadOnlyZipVolume;

public class WarrantReader {
	ReadOnlyContainer volume;
	String URN = null;
	
	ArrayList<String> assertions = new ArrayList<String>();
	
	public WarrantReader(ReadOnlyContainer v, String u) {
		volume = v;
		URN = u;
	}
	
	public ArrayList<String> getAssertions() {
		return assertions;
	}
	
	public boolean isValid() throws IOException, NoSuchAlgorithmException, InvalidKeyException, SignatureException, TooManyValuesException, ParseException {

			String warrantURN = URN;
			String authority = QueryTools.queryValue(volume, null, warrantURN, "aff4:authority");

			boolean verified = false;
			
			String warrantGraph = warrantURN + "/properties";
			QuadList signedStatements = volume.query(warrantGraph, null, null, null);
			GraphCanonicalizer standardiser = new GraphCanonicalizer(signedStatements);
			String canonicalData= standardiser.getCanonicalString();

			byte[] bytes = canonicalData.getBytes("UTF-8");
			AuthorityReader authorityReader = new AuthorityReader(volume,authority);
			
			Signature signature = Signature.getInstance("SHA256withRSA", new BouncyCastleProvider());
			signature.initVerify(authorityReader.publicKey);
			signature.update(bytes);
			
			String sig = QueryTools.queryValue(volume, null, warrantURN + "/properties", "aff4:signature");
			byte[] signatureBytes = Base64.decode(sig);
			if (!signature.verify(signatureBytes)) {
				return false; 
			}

			HashDigestAdapter hasher = new HashDigestAdapter(new SHA256Digest());
			QuadList graphs = volume.query( null, null, "aff4:assertedBy", warrantURN);
			for (Quad graph : graphs) {
				String subjectGraph = graph.getSubject();
				if (!subjectGraph.equals(warrantGraph)) {
					String digestMethod = QueryTools.queryValue(volume, null, subjectGraph, "aff4:digestMethod");
					String digest = QueryTools.queryValue(volume, null, subjectGraph, "aff4:digest");
					
					QuadList statements = volume.query(subjectGraph, null, null, null);
					standardiser = new GraphCanonicalizer(statements);
					hasher.reset();
					hasher.update(standardiser.getCanonicalString());
					hasher.doFinal();
	
					if (!hasher.getStringValue().equals(digest)) {
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