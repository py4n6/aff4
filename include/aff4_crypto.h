/*
** aff4_crypto.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Wed Dec 16 13:48:12 2009 mic
** Last update Wed Dec 16 13:48:30 2009 mic
*/

#ifndef   	AFF4_CRYPTO_H_
# define   	AFF4_CRYPTO_H_

#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/bio.h>
#include <openssl/pem.h>

/** This object represents an identify - like a person for example. 

An identity is someone who makes statements about other AFFObjects in
the universe.
*/
CLASS(Identity, AFFObject)
       Resolver info;
       
       EVP_PKEY *priv_key;
       EVP_PKEY *pub_key;
       X509 *x509;

       Identity METHOD(Identity, Con, char *cert, char *priv_key, char mode);
       void METHOD(Identity, store, char *volume_urn);
  /** This method asks the identity to verify its statements. This
       essentially populates our Resolver with statements which can be
       verified from our statements. Our Resolver can then be
       compared to the oracle to see which objects do not match.
  */
       void METHOD(Identity, verify, int (*cb)(uint64_t progress, char *urn));
END_CLASS

#endif 	    /* !AFF4_CRYPTO_H_ */
