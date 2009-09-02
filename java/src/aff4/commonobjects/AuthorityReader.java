package aff4.commonobjects;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URLEncoder;
import java.security.PublicKey;
import java.security.Security;
import java.security.cert.X509Certificate;
import java.text.ParseException;

import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.bouncycastle.openssl.PEMReader;

import aff4.container.ReadOnlyContainer;
import aff4.datamodel.Reader;
import aff4.datamodel.ReaderInputStream;
import aff4.infomodel.Node;
import aff4.infomodel.Resource;
import aff4.storage.zip.ReadOnlyZipVolume;

public class AuthorityReader {
	ReadOnlyContainer volume;
	PublicKey publicKey = null;
	Resource URN = null;
	
	public AuthorityReader(ReadOnlyContainer v, Resource URN) throws IOException, ParseException {
		volume = v;
		this.URN = URN;
		init();
	}
	
	public void init() throws IOException, ParseException {
		Security.addProvider(new BouncyCastleProvider());
		Reader dev = volume.open(Node.createURI(URN.getURI() + "/cert.pem"));
		PEMReader pemReader = new PEMReader(new InputStreamReader(new ReaderInputStream(dev)));
		X509Certificate cert = (X509Certificate)pemReader.readObject();
		dev.close();

		publicKey = cert.getPublicKey();
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