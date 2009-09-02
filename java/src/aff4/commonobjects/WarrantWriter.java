package aff4.commonobjects;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.Security;
import java.security.Signature;
import java.security.SignatureException;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.UUID;

import org.bouncycastle.crypto.Digest;
import org.bouncycastle.crypto.digests.SHA256Digest;
import org.bouncycastle.crypto.digests.SHA512Digest;
import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.bouncycastle.util.encoders.Base64;
import org.bouncycastle.util.encoders.Hex;

import aff4.container.Container;
import aff4.container.WritableStore;
import aff4.hash.HashDigestAdapter;
import aff4.infomodel.GraphCanonicalizer;
import aff4.infomodel.Node;
import aff4.infomodel.QuadList;
import aff4.infomodel.Resource;
import aff4.infomodel.datatypes.AFF4Datatype;
import aff4.infomodel.datatypes.DataType;
import aff4.infomodel.lexicon.AFF4;
import aff4.infomodel.serialization.PropertiesWriter;
import aff4.storage.zip.WritableZipVolume;

public class WarrantWriter extends ReadWriteInstance{
	ArrayList<Resource> assertions = new ArrayList<Resource>();
	AuthorityWriter authority = null;
	Resource warrantGraph = null;
	
	public WarrantWriter(WritableStore v) {
		super(v);

		warrantGraph = Node.createURI("urn:aff4:" + UUID.randomUUID().toString());

	}

	public void addAssertion(Resource graph) {
		assertions.add(graph);
	}
	
	public void setAuthority(AuthorityWriter authority) {
		this.authority = authority;
	}
	
	public static String calculateSignature(String canonicalGraph, PrivateKey key) throws NoSuchAlgorithmException, InvalidKeyException, SignatureException, UnsupportedEncodingException

	{
		Security.addProvider(new BouncyCastleProvider());
		String signature = null;
		Signature sig = Signature.getInstance("SHA256withRSA", new BouncyCastleProvider());
		sig.initSign(key);
		sig.update(canonicalGraph.getBytes("UTF-8"));
		signature = new String(Base64.encode(sig.sign()),"UTF-8");
		return signature;
	}
	
	static String  digest(String data) throws UnsupportedEncodingException {
    	Security.addProvider( new BouncyCastleProvider() );
		HashDigestAdapter digest = new HashDigestAdapter(new SHA256Digest());
        digest.update( data);
        digest.doFinal();
		
        return digest.getStringValue();
	}
	
	public void close() throws FileNotFoundException, IOException, InvalidKeyException, NoSuchAlgorithmException, SignatureException, ParseException {
		Resource warrant = getURN();
		for (Resource assertion : assertions) {
			QuadList statements = container.query(assertion, Node.ANY, Node.ANY, Node.ANY);
			GraphCanonicalizer standardiser = new GraphCanonicalizer(statements);
			String canonicalData= standardiser.getCanonicalString();
			
			
			String digest = digest(canonicalData);
			container.add(warrantGraph , assertion, AFF4.hash, Node.createLiteral(digest, null,AFF4Datatype.canonical_sha256));
			container.add(warrantGraph , assertion, AFF4.assertedBy, warrant );

		}
		container.add(warrantGraph, warrant , AFF4.authority, authority.getURN());
		container.add(warrantGraph, warrant , AFF4.type, AFF4.Warrant);
		container.add(warrantGraph, warrantGraph, AFF4.assertedBy, warrant );
		//container.add(warrantGraph, warrant, AFF4.signatureMethod, Node.createLiteral("canonical-sha256-rsa", null, a));
		
		QuadList statements = container.query(warrantGraph, null, null, null);
		GraphCanonicalizer standardiser = new GraphCanonicalizer(statements);
		StringBuffer canonicalData = new StringBuffer();
		for (String line: standardiser.getCanonicalStringsArray()) {
			canonicalData.append(line);
			canonicalData.append(",");
		}

		container.add(warrantGraph, warrantGraph, AFF4.signature, Node.createLiteral(calculateSignature(canonicalData.toString(), authority.getPrivateKey()),null,AFF4Datatype.canonical_sha256_rsa));

		/*
		String name = URLEncoder.encode(URN, "UTF-8")	+ "/properties";
		
		PropertiesWriter writer = new PropertiesWriter(URN);
		String res = writer
					.write(container.query(URN + "/properties", null, null, null));
		OutputStream f = container.createOutputStream(name, true, res.length());
		f.write(res.getBytes());
		f.close();
		*/

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
