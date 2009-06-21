/** An identity is someone who makes statements about other AFFObjects in
    the universe.
    
    AFF4 implements signing by use of identities. The identity makes
    statements about the hashes of various other objects, and then
    signs a statement about it. If the identity is trusted, the
    statements can be believed and compared with the true
    hashes. This allows us to detect modification or manipulation of
    evidence.
*/
#include "zip.h"
#include "encode.h"

static void verify_hashes(Identity self, int (*cb)(uint64_t progress, char *urn));

/** This function is a resolver filter which only allows SHA
    attributes. 
*/
static void Identity_Resolver_add(Resolver self, char *uri, char *attribute, char *value) {
  /* we only care about hashes here */
  if(strstr(attribute, AFF4_SHA) == attribute) {
    char *hash = CALL(self, resolve, uri, attribute);

    if(hash && strcmp(hash, value)) {
      printf("**** Hash for %s is incorrect according to %s\n", uri, URNOF(self));
    } else {
      // Call the proper resolver add method
      __Resolver.add(self, uri, attribute, value);
    };
  };
};

// Never delete any information identities hold.
static void Identity_Resolver_del(Resolver self, char *uri, char *attribute) {
  
};

static int Identity_load_certs(Identity self, char *cert, char *priv_key) {
  FileLikeObject fd = NULL;

  /** Load private key info */
  if(priv_key) {
    fd = (FileLikeObject)CALL(oracle, open, priv_key, 'r');
    if(fd && fd->size < 1e6) {
      char *text = talloc_size(fd, fd->size);
      BIO *mem;
      
      CALL(fd, seek, 0, 0);
      CALL(fd, read, text, fd->size);
      
      // Ok now load the cert into the mem BIO:
      mem = BIO_new_mem_buf(text, fd->size);
      self->priv_key = PEM_read_bio_PrivateKey(mem,0,0,NULL);
      if(!self->priv_key) {
	RaiseError(ERuntimeError,"Unable to read private key (%s)", priv_key);
	goto error;
      } else {
	LogWarnings("Will now sign using private key %s", priv_key);
      };

      BIO_set_close(mem, BIO_NOCLOSE); /* So BIO_free() leaves BUF_MEM
					  alone */
      BIO_free(mem);
      talloc_free(text);
      CALL(oracle, cache_return, (AFFObject)fd);
    } else {
      goto error;
    };
  };

  /** Load the certificate */
  if(cert) {
    fd = (FileLikeObject)CALL(oracle, open, cert, 'r');
    if(fd && fd->size < 1e6) {
      char *text = talloc_size(fd, fd->size);
      BIO *mem;
      
      CALL(fd, seek, 0, 0);
      CALL(fd, read, text, fd->size);
      
      // Ok now load the cert into the mem BIO:
      mem = BIO_new_mem_buf(text, fd->size);
      self->x509 = PEM_read_bio_X509(mem,0,0,NULL);
      if(!self->x509) {
	RaiseError(ERuntimeError, "Unable to read x509 certificate from %s", cert);
	goto error;
      } else {
	LogWarnings("Will use public key from %s to verify signatures", cert);
      };

      self->pub_key = X509_get_pubkey(self->x509);

      BIO_set_close(mem, BIO_NOCLOSE); /* So BIO_free() leaves BUF_MEM
					  alone */
      BIO_free(mem);
      talloc_free(text);
      CALL(oracle, cache_return, (AFFObject)fd);
    } else {
      RaiseError(ERuntimeError, "Unable to open file %s", cert);
      goto error;
    };
  };

  /* Check that they actually match. We sign and verify them. */
  if(self->pub_key && self->priv_key) {
    char ptext[]="This is a test message";
    unsigned char sig[1024];            /* signature; bigger than
					   needed */
    unsigned int siglen = sizeof(sig);  /* length of signature */
    EVP_MD_CTX md;                      /* EVP message digest */
    
    /* make the plaintext message */
    EVP_SignInit(&md,EVP_sha256());
    EVP_SignUpdate(&md, ptext, strlen(ptext));
    EVP_SignFinal(&md,sig,&siglen,self->priv_key);
    
    /* Verify the message */
    EVP_VerifyInit(&md,EVP_sha256());
    EVP_VerifyUpdate(&md, ptext, strlen(ptext));
    if(EVP_VerifyFinal(&md,sig,siglen,self->pub_key)!=1) {
      RaiseError(ERuntimeError, "Public (%s) and private "
		 "(%s) keys do not appear to be related",
		 cert, priv_key);
      goto error;
    };
  };
    
  return 0;

 error:
  return -1;
};

/** This is the destructor for the Identity */
static int Identity_destroy(void *self) {
  /* We do not want to be freed
     Identity this = (Identity)self;

     if(this->x509) X509_free(this->x509);
     if(this->priv_key) EVP_PKEY_free(this->priv_key);
     if(this->pub_key) EVP_PKEY_free(this->pub_key);
  */

  // Refuse to free this memory. Once we get installed, we do not free
  // ourselves at all. 
  return -1;
};

static Identity Identity_Con(Identity self, char *cert, char *priv_key, char mode) {
  Identity this = (Identity)self;
  char name[BUFF_SIZE];

  if(!cert) {
    RaiseError(ERuntimeError,"You must specify a certificate");
    goto error;
  };

  if(mode == 'w' && !priv_key) {
    RaiseError(ERuntimeError, "You must specify a priv_key in order to create Identity");
    goto error;
  };

  // Set a destructor to ensure keys get cleaned up when needed.
  if(Identity_load_certs(this, cert, priv_key) < 0)
    goto error;

  /** We set our URN from the fingerprint of the public key */
  {
    unsigned char hash[BUFF_SIZE];
    unsigned char buff[BUFF_SIZE];
    unsigned int hash_len = BUFF_SIZE;
    EVP_MD *md = EVP_sha1();
    int i;

    /* make the plaintext message */
    X509_digest(self->x509, md, hash, &hash_len);
    for(i=0;i<hash_len;i++) {
      sprintf(buff + i*3, "%02x:", hash[i]);
    };
    buff[i*3-1]=0;

    // Make sure the name is valid to be used in a URN
    URNOF(self) = talloc_asprintf(self, AFF4_IDENTITY_PREFIX "/%s", buff);
  };

  /** Now we adjust our URN from the certs */
  if(X509_NAME_oneline(X509_get_subject_name(this->x509), name, sizeof(name))) {
    CALL(oracle, set, URNOF(self), AFF4_COMMON_NAME, name);
  };


  this->info = CONSTRUCT(Resolver, Resolver, Con, oracle);
  // We place a filtering hook for this identity.
  this->info->add = Identity_Resolver_add;
  
  URNOF(this->info) = talloc_strdup(this->info, URNOF(self));
  list_add_tail(&this->info->identities, &oracle->identities);
  
  // Attach ourselves to the resolver so we dont get freed.
  this->info->identity = this;
  talloc_steal(this->info, self);

  talloc_set_destructor((void *)self, Identity_destroy);
  return self;
 error:
  talloc_free(self);
  return NULL;
};

static AFFObject Identity_AFFObject_Con(AFFObject self, char *uri, char mode) {
  Identity this = (Identity)self;
  Resolver i;
  char *priv_key;

  if(!uri) {
    RaiseError(ERuntimeError, "You must construct the Identity yourself");
    goto error;
  };

  priv_key = CALL(oracle, resolve, uri, AFF4_PRIV_KEY);
  this->info = NULL;
  list_for_each_entry(i, &oracle->identities, identities) {
    if(!strcmp(URNOF(i), uri)) {
      /* If we find the identity attached to the oracle we just
	 return it. 
      */
      talloc_free(self);
      return (AFFObject)(i->identity);
    };
  };
  
  return (AFFObject)CALL(this, Con, fully_qualified_name(self, "cert.pem", uri),
			 NULL, 'r');

 error:
  talloc_free(self);
  return NULL;
};

static AFFObject Identity_finish(AFFObject self) {
  return CALL(self, Con, self->urn, 'w');
};

static void Identity_store(Identity self, char *volume_urn) {
  ZipFile volume = (ZipFile)CALL(oracle, open, volume_urn, 'w');
  char *text;
  char filename[BUFF_SIZE];
  char buff[BUFF_SIZE];
  int buff_len=BUFF_SIZE;
  int i=0;
  char *fqn = fully_qualified_name(self, URNOF(self), volume_urn);
  EVP_MD_CTX md;
  FileLikeObject fd;

  EVP_SignInit(&md,EVP_sha256());

  if(!volume) {
    RaiseError(ERuntimeError, "Unable to open volume %s", volume_urn);
    return;
  };

  // Try to find a signing segment
  while(1) {
    snprintf(filename, BUFF_SIZE, "%08d", i);
    if(!CALL(oracle, is_set, fqn, AFF4_STATEMENT, filename))
      break;
    i++;
  };

  CALL(oracle, add, fqn, AFF4_TYPE, AFF4_IDENTITY);
  CALL(oracle, add, fqn, AFF4_STATEMENT, filename);
  PrintError();

  snprintf(filename, BUFF_SIZE, "%s/%08d", URNOF(self), i);
  text = CALL(self->info, export_all, URNOF(self));
  CALL(volume, writestr,
       filename, ZSTRING_NO_NULL(text),
       NULL, 0, 
       ZIP_STORED);

  // Sign the file
  EVP_SignUpdate(&md, text, strlen(text));
  EVP_SignFinal(&md,(unsigned char *)buff,(unsigned int *)&buff_len,self->priv_key);
  snprintf(filename, BUFF_SIZE, "%s/%08d.sig", URNOF(self), i);
  CALL(volume, writestr,
       filename, buff, buff_len,
       NULL, 0, 
       ZIP_STORED);
  
  talloc_free(text);

  // Store the certificate in the volume - is it already there?
  fd = CALL(volume, open_member, fully_qualified_name(self, "cert.pem", URNOF(self)),
	    'r', NULL, 0, 0);

  if(!fd) {
    char name[BUFF_SIZE];
    BIO *xbp = BIO_new(BIO_s_mem());
    char *cert_name = fully_qualified_name(self, "cert.pem", URNOF(self));
    
    if(X509_NAME_oneline(X509_get_subject_name(self->x509), name, sizeof(name))) {
      fd = CALL(volume, open_member, cert_name,
		'w', NULL, 0, 0);

      X509_print_ex(xbp, self->x509, 0, 0);
      PEM_write_bio_X509(xbp,self->x509);

      while(1) {
	int len = BIO_read(xbp, buff, BUFF_SIZE);
	if(len<=0) break;

	CALL(fd, write, buff, len);
      };

      CALL(oracle, add, URNOF(self), AFF4_CERT, cert_name);
    };
    BIO_free(xbp);
  };

  CALL(fd, close);

  // Make a properties file
  text = CALL(oracle, export_urn, URNOF(self), URNOF(self));
  if(text) {
    snprintf(filename, BUFF_SIZE, "%s/properties", URNOF(self));
    CALL(volume, writestr, filename, ZSTRING_NO_NULL(text),
	 NULL, 0, ZIP_STORED);

    talloc_free(text);
  };
};

/** Here we verify our statements.

This means we iterate over all statements, checking the signatures,
and if correct we parse the statements and add them into our resolver.

After this call its possible to check which statements are different
from the oracle.

The callback function will be called periodically (every BUFF_SIZE)
with an indication of the current progress and the currently processed
URN.
*/
static void Identity_verify(Identity self, int (*cb)(uint64_t progress, char *urn)) {
  char **statements = CALL(oracle, resolve_list, NULL, URNOF(self), AFF4_STATEMENT);
  char **statement_urn;
  
  for(statement_urn=statements; *statement_urn; statement_urn++) {
    char buff[BUFF_SIZE];
    char *fq_name = fully_qualified_name(self, *statement_urn, URNOF(self));
    FileLikeObject statement = (FileLikeObject)CALL(oracle, open, fq_name,'r');
			   
    EVP_MD_CTX md;
    FileLikeObject oracle_signature;

    if(!statement) {
      RaiseError(ERuntimeError, "Unable to open statement %s", *statement);
      continue;
    };

    snprintf(buff, BUFF_SIZE, "%s.sig", fq_name);
    oracle_signature = (FileLikeObject)CALL(oracle, open, buff,'r');
    if(!oracle_signature) {
      RaiseError(ERuntimeError, "Unable to open statement signature '%s', skipping.", buff);
      goto error_continue;
    };

    // Now verify the statement signature:
    CALL(statement, get_data);
    CALL(oracle_signature, get_data);

    EVP_VerifyInit(&md,EVP_sha256());
    EVP_VerifyUpdate(&md, statement->data, statement->size);
    if(EVP_VerifyFinal(&md, (unsigned char *)oracle_signature->data,
		       (unsigned int)oracle_signature->size, self->pub_key)!=1) {
      RaiseError(ERuntimeError, "Statement %s does not verify. Skipping it.", buff);
      goto error_continue;
    };
    
    // Ok so the statement itself is ok - so now we need to parse it
    // and load it into our specific identity:
    CALL(self->info, parse, URNOF(self), statement->data,
	 statement->size);

    // Note that by default hashes are not calculated - we need to
    // calculate them here and confirm they are good:
    verify_hashes(self, cb);

    // Ok - done with those now:
    CALL(oracle, cache_return, (AFFObject)oracle_signature);
  error_continue:
    CALL(oracle, cache_return, (AFFObject)statement);
  };
  
  talloc_free(statements);
};

static void verify_hashes(Identity self, int (*cb)(uint64_t progress, char *urn)) {
  Cache i;

  list_for_each_entry(i, &self->info->urn->cache_list, cache_list) {
    char *urn = (char *)i->key;
    char *hash = CALL(self->info, resolve, urn, AFF4_SHA);

    if(hash) {
      FileLikeObject fd = (FileLikeObject)CALL(oracle, open, urn, 'r');
      unsigned char buff[BUFF_SIZE];
      unsigned int len;
      uint64_t progress=0;
      EVP_MD_CTX digest;
      unsigned char hash_base64[BUFF_SIZE];

      if(fd) {
	EVP_DigestInit(&digest, EVP_sha256());
	while(1) {
	  len = CALL(fd, read, (char *)buff, BUFF_SIZE);
	  if(len==0) break;
	  progress+=len;

	  // A zero return code break out of the verification process
	  if(!cb(progress, urn))
	    return;
	  
	  EVP_DigestUpdate(&digest, buff, len);
	};

	memset(buff, 0, BUFF_SIZE);
	EVP_DigestFinal(&digest, buff,&len);
	encode64(buff, len, hash_base64, sizeof(hash_base64));

	CALL(oracle, set, urn, AFF4_SHA, (char *)hash_base64);
      };
    };
  };
  
  // Make the resolver read only now. Basically we allow the code
  // previous to this to update the hashes from later statements. If a
  // file was modified and the new hash is signed in a later statement
  // its ok to update it. Once we finish here, hashes should not be
  // allows to be updated. We make the Identity resolver read only by
  // substituting a NULL del method.
  self->info->del = Identity_Resolver_del;
};

VIRTUAL(Identity, AFFObject)
     VMETHOD(super.Con) = Identity_AFFObject_Con;
     VMETHOD(Con) = Identity_Con;
     VMETHOD(verify) = Identity_verify;
     VMETHOD(store) = Identity_store;
     VMETHOD(super.finish) = Identity_finish;
END_VIRTUAL
