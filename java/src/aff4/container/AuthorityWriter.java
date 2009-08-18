package aff4.container;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.URLEncoder;
import java.security.KeyPair;
import java.security.PrivateKey;
import java.security.Security;
import java.security.cert.X509Certificate;

import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.bouncycastle.openssl.PEMReader;

import aff4.deprecated.FlatWriteDevice;
import aff4.infomodel.serialization.PropertiesWriter;
import aff4.storage.WritableZipVolumeImpl;

public class AuthorityWriter extends Instance {
	FlatWriteDevice target = null;
	PrivateKey privateKey = null;
	String email = null;
	
	public PrivateKey getPrivateKey() {
		return privateKey;
	}

	public AuthorityWriter(WritableZipVolumeImpl v, String email) {
		super(v);
		this.email = email;
	}

	public void setPrivateCert(String certificateFilePath) throws IOException {
		Security.addProvider(new BouncyCastleProvider());
		InputStream dev = new FileInputStream(certificateFilePath);
		PEMReader pemReader = new PEMReader(new InputStreamReader(dev));
		KeyPair keyPair = (KeyPair) pemReader.readObject();
		
		dev.close();
		privateKey = keyPair.getPrivate();
	}
	
	public void setPublicCert(String certificateFilePath) throws IOException {
		Security.addProvider(new BouncyCastleProvider());
		InputStream dev = new FileInputStream(certificateFilePath);
		PEMReader pemReader = new PEMReader(new InputStreamReader(dev));
		X509Certificate cert = (X509Certificate)pemReader.readObject();
		dev.close();
		dev = new FileInputStream(certificateFilePath);
		
		String URN = getURN();
		String name =  URLEncoder.encode(URN)	+ "/cert.pem";
		
		OutputStream certwriter = volume.createOutputStream(name, true, dev.available());
		byte[] buf = new byte[1024];
		while (dev.available() > 0) {
			int res = dev.read(buf);
			certwriter.write(buf, 0, res);
		}
		dev.close();
		certwriter.close();
		volume.add(URN + "/properties", URN, "aff4:publicKeyCertificate", URN + "/cert.pem");
		//PublicKey pk = cert.getPublicKey();
	}
	
	public void close() throws FileNotFoundException, IOException {
		String URN = getURN();
		volume.add(URN + "/properties", URN, "aff4:type", "identity");
		volume.add(URN + "/properties", URN, "aff4:stored", volume.getURN());
		volume.add(URN + "/properties", URN, "foaf:mbox", email);


		volume.add(volume.getURN(), volume.getURN(), "aff4:contains", URN + "/properties");
		
		String name =  URLEncoder.encode(URN)	+ "/properties";
		
		PropertiesWriter writer = new PropertiesWriter(URN);
		String res = writer
				.write(volume.getQuadStore().query(URN + "/properties", URN, null, null));
		OutputStream f = volume.createOutputStream(name, true, res.length());
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
