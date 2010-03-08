import pyaff4
import os, time, sys

time.sleep(1)

oracle = pyaff4.Resolver()

class SecurityProvider:
    """ This is a demonstration security provider object which will be
    called by the AFF4 library to get keying material for different
    streams.
    """
    CERT_LOCATION = "/tmp/sign.key"

    def make_cert(self):
        try:
            data = open(self.CERT_LOCATION).read()
        except IOError:
            os.system("openssl req -x509 -newkey rsa:1024 -keyout %s -out %s -nodes" %(
                    self.CERT_LOCATION, self.CERT_LOCATION))
            data = open(self.CERT_LOCATION).read()

        return data

    def passphrase(self, cipher, subject):
        print "Setting passphrase for subject %s" % subject.value
        return "Hello"

    def x509_cert(self, cipher, subject):
        """ Returns the location of the x509 certificate to use with the subject """
        self.make_cert()
        ## This is a test - we just use a self signed pair
        return "file://%s" % self.CERT_LOCATION

    def x509_private_key(self, cert_name):
        """ Returns the private key (in pem format) for the certificate name provided. """
        return self.make_cert()

## This registers the security provider
oracle.register_security_provider(pyaff4.ProxiedSecurityProvider(SecurityProvider()))

url = pyaff4.RDFURN()
url.set("/tmp/test.zip")

try:
    url.set(sys.argv[1])
    fd = oracle.open(url, 'r')
    while 1:
        data = fd.read(1024*1024)
        if not data: break

        sys.stdout.write(data)

    sys.exit(0)
except IndexError:
    pass

try:
    os.unlink(url.parser.query)
except: pass

## Make the volume
volume = oracle.create(pyaff4.AFF4_ZIP_VOLUME)
volume.set(pyaff4.AFF4_STORED, url)
volume = volume.finish()
volume_urn = volume.urn
volume.cache_return()

## Make the image
image = oracle.create(pyaff4.AFF4_IMAGE)
image.set(pyaff4.AFF4_STORED, volume_urn)
image = image.finish()
image_urn = image.urn
image.cache_return()

# Make the encrypted stream
encrypted = oracle.create(pyaff4.AFF4_ENCRYTED)
encrypted.set(pyaff4.AFF4_STORED, volume_urn)
encrypted.set(pyaff4.AFF4_TARGET, image_urn)

## Set the password - this will invoke the security manager to key the
## cipher.
#cipher = oracle.new_rdfvalue(pyaff4.AFF4_AES256_X509)
cipher = oracle.new_rdfvalue(pyaff4.AFF4_AES256_PASSWORD)
encrypted.set(pyaff4.AFF4_CIPHER, cipher)

encrypted = encrypted.finish()
encrypted_urn = encrypted.urn

print "Encrypted URN: %s" % encrypted.urn.value

infd = open("/bin/ls")
while 1:
    data = infd.read(2**24)
    if not data: break

    encrypted.write(data)

encrypted.close()

image = oracle.open(image_urn, "w")
image.close()

volume = oracle.open(volume_urn, 'w')
volume.close()

## Check the data
fd = oracle.open(encrypted_urn, 'r')
print fd.read(10)
fd.cache_return()
