package aff4.commonobjects;

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
import java.text.ParseException;

import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.bouncycastle.openssl.PEMReader;

import aff4.container.Container;
import aff4.container.WritableStore;
import aff4.infomodel.Node;
import aff4.infomodel.Resource;
import aff4.infomodel.lexicon.AFF4;
import aff4.infomodel.lexicon.FOAF;
import aff4.infomodel.serialization.PropertiesWriter;
import aff4.storage.zip.WritableZipVolume;

public class AuthorityWriter extends ReadWriteInstance {
	PrivateKey privateKey = null;
	String email = null;
	
	public PrivateKey getPrivateKey() {
		return privateKey;
	}

	public AuthorityWriter(WritableStore v, String email) {
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
		dev.close();
		dev = new FileInputStream(certificateFilePath);
		
		Resource URN = getURN();
		String name =  URLEncoder.encode(URN.getURI(), "UTF-8")	+ "/cert.pem";
		
		OutputStream certwriter = container.createOutputStream(name, true, dev.available());
		byte[] buf = new byte[1024];
		while (dev.available() > 0) {
			int res = dev.read(buf);
			certwriter.write(buf, 0, res);
		}
		dev.close();
		certwriter.close();
		container.add(container.getGraph(), URN, AFF4.publicKeyCertificate, Node.createURI(URN.getURI() + "/cert.pem"));
		//PublicKey pk = cert.getPublicKey();
	}
	
	public void close() throws FileNotFoundException, IOException, ParseException {
		Resource URN = getURN();
		container.add(container.getGraph(), URN, AFF4.type, AFF4.identity);
		container.add(container.getGraph(), URN, AFF4.stored, container.getURN());
		container.add(container.getGraph(), URN, FOAF.mbox, Node.createLiteral(email, null, null));


		//container.add(container.getURN(), container.getURN(), "aff4:contains", URN + "/properties");
		/*
		String name =  URLEncoder.encode(URN, "UTF-8")	+ "/properties";
		
		PropertiesWriter writer = new PropertiesWriter(URN);
		String res = writer
				.write(container.query(URN + "/properties", URN, null, null));
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
