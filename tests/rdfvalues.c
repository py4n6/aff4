/*************************************************
 This file tests various RDFValues.
***************************************************/

#include "aff4.h"

/********************************************
   Tests for URL Parsing.
********************************************/

TEST(URLParserTest) {
  RDFURN url = new_RDFURN(NULL);

  url->set(url, "http://www.google.com/path/to/nowhere?query=foo#frag1");

  CU_ASSERT_STRING_EQUAL(url->parser->scheme, "http");
  CU_ASSERT_STRING_EQUAL(url->parser->netloc, "www.google.com");
  CU_ASSERT_STRING_EQUAL(url->parser->query, "/path/to/nowhere?query=foo");
  CU_ASSERT_STRING_EQUAL(url->parser->fragment, "frag1");

  CU_ASSERT_STRING_EQUAL(url->parser->string(url->parser, url),
                         "http://www.google.com/path/to/nowhere?query=foo#frag1");

  aff4_free(url);
};

TEST(FileURLTest) {
  RDFURN url = new_RDFURN(NULL);

  url->set(url, "/tmp/foobar");

  CU_ASSERT_STRING_EQUAL(url->parser->scheme, "");
  CU_ASSERT_STRING_EQUAL(url->parser->netloc, "");
  CU_ASSERT_STRING_EQUAL(url->parser->query, "/tmp/foobar");
  CU_ASSERT_STRING_EQUAL(url->parser->fragment, "");

  CU_ASSERT_STRING_EQUAL(url->parser->string(url->parser, url),
                         "file:///tmp/foobar");
  aff4_free(url);
};

TEST(URLParserTraversal) {
  RDFURN url = new_RDFURN(NULL);

  /* This URL has /../, /./ and // sequences which must be reduced */
  url->set(
      url, "http://www.google.com/path/to/./././//nowhere/../../from/somewhere");

  CU_ASSERT_STRING_EQUAL(url->parser->scheme, "http");
  CU_ASSERT_STRING_EQUAL(url->parser->netloc, "www.google.com");
  CU_ASSERT_STRING_EQUAL(url->parser->query,
                         "/path/to/./././//nowhere/../../from/somewhere");

  CU_ASSERT_STRING_EQUAL(url->parser->string(url->parser, url),
                         "http://www.google.com/path/from/somewhere");

  aff4_free(url);
};


TEST(URLParserTraversal2) {
  RDFURN url = new_RDFURN(NULL);

  /* This URL has /../ elements traversing past the root */
  url->set(
      url, "http://www.google.com/to/../../..///nowhere/../../../from/somewhere");

  CU_ASSERT_STRING_EQUAL(url->parser->string(url->parser, url),
                         "http://www.google.com/from/somewhere");

  aff4_free(url);
};

/********************************************
   Tests for XSDInteger
********************************************/

TEST(XSDIntegerTest) {
  // Our integer is our memory context
  XSDInteger i = new_XSDInteger(NULL);
  RDFURN subject = new_RDFURN(i);
  TDB_DATA *data;
  uint64_t value = 12345;
  // This is an alias to the base class
  RDFValue base = (RDFValue)i;

  subject->set(subject, "aff4://12345/subject");

  i->set(i, value);
  CU_ASSERT_EQUAL(i->value, value);

  // Encoding
  data = base->encode(base, subject);
  CU_ASSERT_TRUE(!memcmp((char *)data->dptr, (char *)&value, sizeof(value)));
  CU_ASSERT_EQUAL(data->dsize, sizeof(value));
  CU_ASSERT_STRING_EQUAL(base->serialise(base, subject), "12345");

  // Decoding
  value = 54321;
  base->decode(base, (char *)&value, sizeof(value), subject);
  CU_ASSERT_EQUAL(i->value, value);
  CU_ASSERT_STRING_EQUAL(base->serialise(base, subject), "54321");

  // Parsing
  base->parse(base, "656567", subject);
  CU_ASSERT_EQUAL(i->value, 656567);

  // This is sufficient - all other memory will be freed here.
  aff4_free(i);
};


/********************************************
   Tests for XSDString
********************************************/

TEST(XSDStringTest) {
  // Our string is our memory context
  XSDString i = new_XSDString(NULL);
  RDFURN subject = new_RDFURN(i);
  TDB_DATA *data;
  char *value = "this is a test";
  int value_len = strlen(value);
  // This is an alias to the base class
  RDFValue base = (RDFValue)i;

  subject->set(subject, "aff4://12345/subject");

  i->set(i, value, value_len);
  CU_ASSERT_STRING_EQUAL(i->value, value);
  CU_ASSERT_EQUAL(i->length, value_len);

  // Encoding
  data = base->encode(base, subject);
  CU_ASSERT_TRUE(!memcmp((char *)data->dptr, value, value_len));
  CU_ASSERT_EQUAL(data->dsize, value_len);
  CU_ASSERT_STRING_EQUAL(base->serialise(base, subject), value);

  // Decoding
  value = "another string";
  value_len = strlen(value);

  base->decode(base, value, value_len, subject);
  CU_ASSERT_STRING_EQUAL(i->value, value);
  CU_ASSERT_STRING_EQUAL(base->serialise(base, subject), value);


  // Clone
  {
    XSDString clone = (XSDString)i->super.clone((RDFValue)i, i);

    CU_ASSERT_STRING_EQUAL(i->value, clone->value);
  };

  // This is sufficient - all other memory will be freed here.
  aff4_free(i);
};


/********************************************
   Tests for RDFURN
********************************************/

TEST(RDFURNTest) {
  RDFURN url = new_RDFURN(NULL);
  RDFURN subject = new_RDFURN(url);
  TDB_DATA *data;
  char *value = "aff4://8768765486/root/file";
  int value_len = strlen(value);
  // This is an alias to the base class
  RDFValue base = (RDFValue)url;
  Resolver oracle = AFF4_get_resolver();

  subject->set(subject, "aff4://12345/subject");
  url->set(url, value);

  CU_ASSERT_STRING_EQUAL(url->value, value);

  // Encoding
  data = base->encode(base, subject);

  // We encode the URN id as a uint32_t
  CU_ASSERT_EQUAL(*(uint32_t *)data->dptr,
                  oracle->get_id_by_urn(oracle, url, 0));
  CU_ASSERT_EQUAL(data->dsize, sizeof(uint32_t));

  CU_ASSERT_STRING_EQUAL(base->serialise(base, subject), value);


  // Add relative
  url->add(url, "foo");
  CU_ASSERT_STRING_EQUAL(url->value, "aff4://8768765486/root/file/foo");

  // Add absolute
  url->add(url, "aff4://121/a");
  CU_ASSERT_STRING_EQUAL(url->value, "aff4://121/a");

  // Add query
  url->add_query(url, (unsigned char *)"\x12#&%\x34/foo", 9);
  CU_ASSERT_STRING_EQUAL(url->value, "aff4://121/a/%12%23%26%254/foo");

  // Decoding
  {
    uint32_t urn_id = oracle->get_id_by_urn(oracle, subject, 1);

    base->decode(base, (char *)&urn_id, sizeof(urn_id), subject);

    CU_ASSERT_STRING_EQUAL(url->value, subject->value);
  };

  // Clone
  {
    RDFURN clone = (RDFURN)url->super.clone((RDFValue)url, url);

    CU_ASSERT_STRING_EQUAL(url->value, clone->value);
  };

  // This is sufficient - all other memory will be freed here.
  aff4_free(url);
};
