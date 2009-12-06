/*
** exports.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Fri Dec  4 17:14:21 2009 mic
** Last update Fri Dec  4 17:14:21 2009 mic
*/

#ifndef   	EXPORTS_H_
# define   	EXPORTS_H_

#pragma GCC visibility push(default)
extern Resolver oracle;
extern void aff4_free(void *ptr);
extern char *_traceback;

extern void AFF4_Init(void);

extern void register_rdf_value_class(RDFValue class_ref);
extern RDFValue rdfvalue_from_int(void *ctx, uint64_t value);
extern RDFValue rdfvalue_from_urn(void *ctx, char *value);
extern RDFValue rdfvalue_from_string(void *ctx, char *value);

extern RDFURN new_RDFURN(void *ctx);
extern XSDInteger new_XSDInteger(void *ctx);
extern XSDString new_XSDString(void *ctx);
extern XSDDatetime new_XSDDateTime(void *ctx);

#pragma GCC visibility pop


#endif 	    /* !EXPORTS_H_ */
