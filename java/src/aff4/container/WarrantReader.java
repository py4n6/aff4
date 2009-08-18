package aff4.container;

import java.io.IOException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.Signature;
import java.security.SignatureException;
import java.text.ParseException;

import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.bouncycastle.util.encoders.Base64;

import aff4.infomodel.GraphCanonicalizer;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.TooManyValuesException;
import aff4.storage.ReadOnlyZipVolume;

public class WarrantReader {
	ReadOnlyZipVolume volume;
	String URN = null;
	public WarrantReader(ReadOnlyZipVolume v, String u) {
		volume = v;
		URN = u;
	}
	
	public boolean isValid() throws IOException, NoSuchAlgorithmException, InvalidKeyException, SignatureException, TooManyValuesException, ParseException {

			String warrantURN = URN;
			String authority = volume.queryValue(null, warrantURN, "aff4:authority");
			QuadList graphs = volume.query(null, null, "aff4:assertedBy", warrantURN);
			for (Quad graph : graphs) {
				String subjectGraph = graph.getSubject();
				QuadList signedStatements = volume.query(warrantURN + "/properties", null, null, null);
				
				GraphCanonicalizer standardiser = new GraphCanonicalizer(signedStatements);
				String canonicalData= standardiser.getCanonicalString();
			
				System.out.println(canonicalData);

				byte[] bytes = canonicalData.getBytes("UTF-8");
				AuthorityReader authorityReader = new AuthorityReader(volume,authority);
				Signature signature = Signature.getInstance("SHA256withRSA", new BouncyCastleProvider());
				signature.initVerify(authorityReader.publicKey);
				signature.update(bytes);
				
				String sig = volume.queryValue(null, warrantURN + "/properties", "aff4:signature");
				byte[] signatureBytes = Base64.decode(sig);
				boolean verified = signature.verify(signatureBytes); 
				return verified;
			}

		return false;
		
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