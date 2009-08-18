package aff4.scratch;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.ByteBuffer;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.PublicKey;
import java.security.Security;
import java.security.Signature;
import java.security.SignatureException;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;

import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.bouncycastle.openssl.PEMReader;

import de.schlichtherle.util.zip.ZipEntry;
import de.schlichtherle.util.zip.ZipFile;

import aff4.storage.ReadOnlyZipVolume;
import aff4.util.HexFormatter;
import junit.framework.TestCase;

public class TestCrypto extends TestCase {
	public void testX509() {
			try {
				Security.addProvider(new BouncyCastleProvider());
				
				ZipFile v = new ZipFile("C:\\mysrc\\AFF4.1\\aff4\\python\\test3.zip");
				//ZipEntry e = v.getEntry(name)
				InputStream dev = v.getInputStream("urn%3Aaff4%3Aidentity/71ACE85E261A1E2EB2858F85B63CCECE/cert.pem", true);
				PEMReader pemReader = new PEMReader(new InputStreamReader(dev));
				X509Certificate cert = (X509Certificate)pemReader.readObject();
				dev.close();
				
				PublicKey pk = cert.getPublicKey();
				
				InputStream s = v.getInputStream("urn%3Aaff4%3Aidentity/71ACE85E261A1E2EB2858F85B63CCECE/5d4ab323-0c6e-40e9-a110-9ffa7bff6d81", true);
				byte[] statementBytes = new byte[1024];
				int countStatementBytes = s.read(statementBytes);
				s.close();
				
				s = v.getInputStream("urn%3Aaff4%3Aidentity/71ACE85E261A1E2EB2858F85B63CCECE/5d4ab323-0c6e-40e9-a110-9ffa7bff6d81.sig", true);
				byte[] signatureBytes = new byte[1024];
				int countsignatureBytes = s.read(signatureBytes);
				
				cert.verify(pk);
				
				
			
				Signature signature = Signature.getInstance(cert.getSigAlgName());
				signature.initVerify(pk);
				signature.update(statementBytes,0,countStatementBytes);
				assertTrue(signature.verify(signatureBytes,0,countsignatureBytes)); 
			} catch (IOException e) {
				fail();
			} catch (SignatureException ex) {
				fail();
			} catch (NoSuchProviderException ex) {
				fail();
			} catch (InvalidKeyException ex) {
				fail();
			} catch (NoSuchAlgorithmException ex) {
				fail();
			} catch (CertificateException ex) {
				fail();
			}
		}

}
