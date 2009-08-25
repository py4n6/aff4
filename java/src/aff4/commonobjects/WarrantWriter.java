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
import aff4.infomodel.QuadList;
import aff4.infomodel.serialization.PropertiesWriter;
import aff4.storage.zip.WritableZipVolume;

public class WarrantWriter extends ReadWriteInstance{
	ArrayList<String> assertions = new ArrayList<String>();
	AuthorityWriter authority = null;
	
	public WarrantWriter(WritableStore v) {
		super(v);
	}

	public void addAssertion(String graph) {
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
		String URN = getURN();
		for (String urn : assertions) {
			QuadList statements = container.query(urn, null, null, null);
			GraphCanonicalizer standardiser = new GraphCanonicalizer(statements);
			String canonicalData= standardiser.getCanonicalString();
			
			
			String digest = digest(canonicalData);
			container.add(URN + "/properties" , urn, "aff4:digestMethod", "canonical-sha256");
			container.add(URN + "/properties" , urn, "aff4:digest", digest);
			container.add(URN + "/properties" , urn, "aff4:assertedBy", URN );

		}
		container.add(URN + "/properties", URN , "aff4:authority", authority.getURN());
		container.add(URN + "/properties", URN , "aff4:type", "aff4:Warrant");
		container.add(URN + "/properties", URN + "/properties", "aff4:assertedBy", URN );
		container.add(URN + "/properties", URN, "aff4:signatureMethod", "canonical-sha256-rsa");
		
		QuadList statements = container.query(URN + "/properties", null, null, null);
		GraphCanonicalizer standardiser = new GraphCanonicalizer(statements);
		StringBuffer canonicalData = new StringBuffer();
		for (String line: standardiser.getCanonicalStringsArray()) {
			canonicalData.append(line);
			canonicalData.append(",");
		}

		container.add(URN + "/properties", URN + "/properties", "aff4:signature", calculateSignature(canonicalData.toString(), authority.getPrivateKey()));

		String name = URLEncoder.encode(URN, "UTF-8")	+ "/properties";
		
		PropertiesWriter writer = new PropertiesWriter(URN);
		String res = writer
					.write(container.query(URN + "/properties", null, null, null));
		OutputStream f = container.createOutputStream(name, true, res.length());
		f.write(res.getBytes());
		f.close();

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
