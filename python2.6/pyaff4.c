#include <Python.h>
#include "/home/mic/projects/aff4/include/aff4_constants.h"
#include "/home/mic/projects/aff4/include/aff4_rdf.h"
#include "/home/mic/projects/aff4/include/aff4_io.h"
#include "/home/mic/projects/aff4/include/aff4_resolver.h"
#include "/home/mic/projects/aff4/include/aff4_objects.h"
#include "/home/mic/projects/aff4/include/aff4_http.h"

/* The following is a static array mapping CLASS() pointers to their
python wrappers. This is used to allow the correct wrapper to be
chosen depending on the object type found - regardless of the
prototype.

This is basically a safer way for us to cast the correct python type
depending on context rather than assuming a type based on the .h
definition. For example consider the function

AFFObject Resolver.open(uri, mode)

The .h file implies that an AFFObject object is returned, but this is
not true as most of the time an object of a derived class will be
returned. In C we cast the returned value to the correct type. In the
python wrapper we just instantiate the correct python object wrapper
at runtime depending on the actual returned type. We use this lookup
table to do so.
*/
static int TOTAL_CLASSES=0;

static struct python_wrapper_map_t {
       Object class_ref;
       PyTypeObject *python_type;
} python_wrappers[22];

/** This is a generic wrapper type */
typedef struct {
  PyObject_HEAD
  void *base;
} Gen_wrapper;

/* Create the relevant wrapper from the item based on the lookup
table.
*/
Gen_wrapper *new_class_wrapper(Object item) {
   int i;
   Gen_wrapper *result;

   for(i=0; i<TOTAL_CLASSES; i++) {
     if(python_wrappers[i].class_ref == item->__class__) {
       result = (Gen_wrapper *)_PyObject_New(python_wrappers[i].python_type);
       result->base = (void *)item;

       return result;
     };
   };

  PyErr_Format(PyExc_RuntimeError, "Unable to find a wrapper for object %s", NAMEOF(item));
  return NULL;
};


typedef struct {
  PyObject_HEAD
  MapDriver base;
} pyMapDriver; 

staticforward PyTypeObject MapDriver_Type;

static int pyMapDriver_init(pyMapDriver *self, PyObject *args, PyObject *kwds);

static PyObject *MapDriver_getattr(pyMapDriver *self, PyObject *name);
static PyObject *pyMapDriver_set_property(pyMapDriver *self, PyObject *args, PyObject *kwds);
static PyObject *pyMapDriver_finish(pyMapDriver *self, PyObject *args, PyObject *kwds);
static PyObject *pyMapDriver_seek(pyMapDriver *self, PyObject *args, PyObject *kwds);
static PyObject *pyMapDriver_read(pyMapDriver *self, PyObject *args, PyObject *kwds);
static PyObject *pyMapDriver_write(pyMapDriver *self, PyObject *args, PyObject *kwds);
static PyObject *pyMapDriver_tell(pyMapDriver *self, PyObject *args, PyObject *kwds);
static PyObject *pyMapDriver_get_data(pyMapDriver *self, PyObject *args, PyObject *kwds);
static PyObject *pyMapDriver_truncate(pyMapDriver *self, PyObject *args, PyObject *kwds);
static PyObject *pyMapDriver_close(pyMapDriver *self, PyObject *args, PyObject *kwds);
static PyObject *pyMapDriver_del(pyMapDriver *self, PyObject *args, PyObject *kwds);
static PyObject *pyMapDriver_add(pyMapDriver *self, PyObject *args, PyObject *kwds);
static PyObject *pyMapDriver_save_map(pyMapDriver *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  AES256Password base;
} pyAES256Password; 

staticforward PyTypeObject AES256Password_Type;

static int pyAES256Password_init(pyAES256Password *self, PyObject *args, PyObject *kwds);

static PyObject *AES256Password_getattr(pyAES256Password *self, PyObject *name);
static PyObject *pyAES256Password_parse(pyAES256Password *self, PyObject *args, PyObject *kwds);
static PyObject *pyAES256Password_encode(pyAES256Password *self, PyObject *args, PyObject *kwds);
static PyObject *pyAES256Password_decode(pyAES256Password *self, PyObject *args, PyObject *kwds);
static PyObject *pyAES256Password_serialise(pyAES256Password *self, PyObject *args, PyObject *kwds);
static PyObject *pyAES256Password_encrypt(pyAES256Password *self, PyObject *args, PyObject *kwds);
static PyObject *pyAES256Password_decrypt(pyAES256Password *self, PyObject *args, PyObject *kwds);
static PyObject *pyAES256Password_set(pyAES256Password *self, PyObject *args, PyObject *kwds);
static PyObject *pyAES256Password_fetch_password_cb(pyAES256Password *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  XSDString base;
} pyXSDString; 

staticforward PyTypeObject XSDString_Type;

static int pyXSDString_init(pyXSDString *self, PyObject *args, PyObject *kwds);

static PyObject *XSDString_getattr(pyXSDString *self, PyObject *name);
static PyObject *pyXSDString_parse(pyXSDString *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDString_encode(pyXSDString *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDString_decode(pyXSDString *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDString_serialise(pyXSDString *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDString_set(pyXSDString *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDString_get(pyXSDString *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  AFFObject base;
} pyAFFObject; 

staticforward PyTypeObject AFFObject_Type;

static int pyAFFObject_init(pyAFFObject *self, PyObject *args, PyObject *kwds);

static PyObject *AFFObject_getattr(pyAFFObject *self, PyObject *name);
static PyObject *pyAFFObject_set_property(pyAFFObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyAFFObject_finish(pyAFFObject *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  Image base;
} pyImage; 

staticforward PyTypeObject Image_Type;

static int pyImage_init(pyImage *self, PyObject *args, PyObject *kwds);

static PyObject *Image_getattr(pyImage *self, PyObject *name);
static PyObject *pyImage_set_property(pyImage *self, PyObject *args, PyObject *kwds);
static PyObject *pyImage_finish(pyImage *self, PyObject *args, PyObject *kwds);
static PyObject *pyImage_seek(pyImage *self, PyObject *args, PyObject *kwds);
static PyObject *pyImage_read(pyImage *self, PyObject *args, PyObject *kwds);
static PyObject *pyImage_write(pyImage *self, PyObject *args, PyObject *kwds);
static PyObject *pyImage_tell(pyImage *self, PyObject *args, PyObject *kwds);
static PyObject *pyImage_get_data(pyImage *self, PyObject *args, PyObject *kwds);
static PyObject *pyImage_truncate(pyImage *self, PyObject *args, PyObject *kwds);
static PyObject *pyImage_close(pyImage *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  Encrypted base;
} pyEncrypted; 

staticforward PyTypeObject Encrypted_Type;

static int pyEncrypted_init(pyEncrypted *self, PyObject *args, PyObject *kwds);

static PyObject *Encrypted_getattr(pyEncrypted *self, PyObject *name);
static PyObject *pyEncrypted_set_property(pyEncrypted *self, PyObject *args, PyObject *kwds);
static PyObject *pyEncrypted_finish(pyEncrypted *self, PyObject *args, PyObject *kwds);
static PyObject *pyEncrypted_seek(pyEncrypted *self, PyObject *args, PyObject *kwds);
static PyObject *pyEncrypted_read(pyEncrypted *self, PyObject *args, PyObject *kwds);
static PyObject *pyEncrypted_write(pyEncrypted *self, PyObject *args, PyObject *kwds);
static PyObject *pyEncrypted_tell(pyEncrypted *self, PyObject *args, PyObject *kwds);
static PyObject *pyEncrypted_get_data(pyEncrypted *self, PyObject *args, PyObject *kwds);
static PyObject *pyEncrypted_truncate(pyEncrypted *self, PyObject *args, PyObject *kwds);
static PyObject *pyEncrypted_close(pyEncrypted *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  ZipFile base;
} pyZipFile; 

staticforward PyTypeObject ZipFile_Type;

static int pyZipFile_init(pyZipFile *self, PyObject *args, PyObject *kwds);

static PyObject *ZipFile_getattr(pyZipFile *self, PyObject *name);
static PyObject *pyZipFile_set_property(pyZipFile *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFile_finish(pyZipFile *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFile_Con(pyZipFile *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFile_open_member(pyZipFile *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFile_close(pyZipFile *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFile_writestr(pyZipFile *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFile_load_from(pyZipFile *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  XSDDatetime base;
} pyXSDDatetime; 

staticforward PyTypeObject XSDDatetime_Type;

static int pyXSDDatetime_init(pyXSDDatetime *self, PyObject *args, PyObject *kwds);

static PyObject *XSDDatetime_getattr(pyXSDDatetime *self, PyObject *name);
static PyObject *pyXSDDatetime_parse(pyXSDDatetime *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDDatetime_encode(pyXSDDatetime *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDDatetime_decode(pyXSDDatetime *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDDatetime_serialise(pyXSDDatetime *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDDatetime_set(pyXSDDatetime *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  URLParse base;
} pyURLParse; 

staticforward PyTypeObject URLParse_Type;

static int pyURLParse_init(pyURLParse *self, PyObject *args, PyObject *kwds);

static PyObject *URLParse_getattr(pyURLParse *self, PyObject *name);
static PyObject *pyURLParse_parse(pyURLParse *self, PyObject *args, PyObject *kwds);
static PyObject *pyURLParse_string(pyURLParse *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  RDFValue base;
} pyRDFValue; 

staticforward PyTypeObject RDFValue_Type;

static int pyRDFValue_init(pyRDFValue *self, PyObject *args, PyObject *kwds);

static PyObject *RDFValue_getattr(pyRDFValue *self, PyObject *name);
static PyObject *pyRDFValue_parse(pyRDFValue *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFValue_encode(pyRDFValue *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFValue_decode(pyRDFValue *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFValue_serialise(pyRDFValue *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  HTTPObject base;
} pyHTTPObject; 

staticforward PyTypeObject HTTPObject_Type;

static int pyHTTPObject_init(pyHTTPObject *self, PyObject *args, PyObject *kwds);

static PyObject *HTTPObject_getattr(pyHTTPObject *self, PyObject *name);
static PyObject *pyHTTPObject_set_property(pyHTTPObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyHTTPObject_finish(pyHTTPObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyHTTPObject_seek(pyHTTPObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyHTTPObject_read(pyHTTPObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyHTTPObject_write(pyHTTPObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyHTTPObject_tell(pyHTTPObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyHTTPObject_get_data(pyHTTPObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyHTTPObject_truncate(pyHTTPObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyHTTPObject_close(pyHTTPObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyHTTPObject_Con(pyHTTPObject *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  ZipFileStream base;
} pyZipFileStream; 

staticforward PyTypeObject ZipFileStream_Type;

static int pyZipFileStream_init(pyZipFileStream *self, PyObject *args, PyObject *kwds);

static PyObject *ZipFileStream_getattr(pyZipFileStream *self, PyObject *name);
static PyObject *pyZipFileStream_set_property(pyZipFileStream *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFileStream_finish(pyZipFileStream *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFileStream_seek(pyZipFileStream *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFileStream_read(pyZipFileStream *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFileStream_write(pyZipFileStream *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFileStream_tell(pyZipFileStream *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFileStream_get_data(pyZipFileStream *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFileStream_truncate(pyZipFileStream *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFileStream_close(pyZipFileStream *self, PyObject *args, PyObject *kwds);
static PyObject *pyZipFileStream_Con(pyZipFileStream *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  Resolver base;
} pyResolver; 

staticforward PyTypeObject Resolver_Type;

static int pyResolver_init(pyResolver *self, PyObject *args, PyObject *kwds);

static PyObject *Resolver_getattr(pyResolver *self, PyObject *name);
static PyObject *pyResolver_open(pyResolver *self, PyObject *args, PyObject *kwds);
static PyObject *pyResolver_cache_return(pyResolver *self, PyObject *args, PyObject *kwds);
static PyObject *pyResolver_create(pyResolver *self, PyObject *args, PyObject *kwds);
static PyObject *pyResolver_resolve_value(pyResolver *self, PyObject *args, PyObject *kwds);
static PyObject *pyResolver_get_iter(pyResolver *self, PyObject *args, PyObject *kwds);
static PyObject *pyResolver_iter_next(pyResolver *self, PyObject *args, PyObject *kwds);
static PyObject *pyResolver_iter_next_alloc(pyResolver *self, PyObject *args, PyObject *kwds);
static PyObject *pyResolver_del(pyResolver *self, PyObject *args, PyObject *kwds);
static PyObject *pyResolver_set_value(pyResolver *self, PyObject *args, PyObject *kwds);
static PyObject *pyResolver_add_value(pyResolver *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  FileBackedObject base;
} pyFileBackedObject; 

staticforward PyTypeObject FileBackedObject_Type;

static int pyFileBackedObject_init(pyFileBackedObject *self, PyObject *args, PyObject *kwds);

static PyObject *FileBackedObject_getattr(pyFileBackedObject *self, PyObject *name);
static PyObject *pyFileBackedObject_set_property(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_finish(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_seek(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_read(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_write(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_tell(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_get_data(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_truncate(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_close(pyFileBackedObject *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  FileLikeObject base;
} pyFileLikeObject; 

staticforward PyTypeObject FileLikeObject_Type;

static int pyFileLikeObject_init(pyFileLikeObject *self, PyObject *args, PyObject *kwds);

static PyObject *FileLikeObject_getattr(pyFileLikeObject *self, PyObject *name);
static PyObject *pyFileLikeObject_set_property(pyFileLikeObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileLikeObject_finish(pyFileLikeObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileLikeObject_seek(pyFileLikeObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileLikeObject_read(pyFileLikeObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileLikeObject_write(pyFileLikeObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileLikeObject_tell(pyFileLikeObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileLikeObject_get_data(pyFileLikeObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileLikeObject_truncate(pyFileLikeObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileLikeObject_close(pyFileLikeObject *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  RESOLVER_ITER *base;
} pyRESOLVER_ITER; 

staticforward PyTypeObject RESOLVER_ITER_Type;

static int pyRESOLVER_ITER_init(pyRESOLVER_ITER *self, PyObject *args, PyObject *kwds);

static PyObject *RESOLVER_ITER_getattr(pyRESOLVER_ITER *self, PyObject *name);

typedef struct {
  PyObject_HEAD
  XSDInteger base;
} pyXSDInteger; 

staticforward PyTypeObject XSDInteger_Type;

static int pyXSDInteger_init(pyXSDInteger *self, PyObject *args, PyObject *kwds);

static PyObject *XSDInteger_getattr(pyXSDInteger *self, PyObject *name);
static PyObject *pyXSDInteger_parse(pyXSDInteger *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDInteger_encode(pyXSDInteger *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDInteger_decode(pyXSDInteger *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDInteger_serialise(pyXSDInteger *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDInteger_set(pyXSDInteger *self, PyObject *args, PyObject *kwds);
static PyObject *pyXSDInteger_get(pyXSDInteger *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  RDFURN base;
} pyRDFURN; 

staticforward PyTypeObject RDFURN_Type;

static int pyRDFURN_init(pyRDFURN *self, PyObject *args, PyObject *kwds);

static PyObject *RDFURN_getattr(pyRDFURN *self, PyObject *name);
static PyObject *pyRDFURN_parse(pyRDFURN *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFURN_encode(pyRDFURN *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFURN_decode(pyRDFURN *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFURN_serialise(pyRDFURN *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFURN_set(pyRDFURN *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFURN_get(pyRDFURN *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFURN_copy(pyRDFURN *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFURN_add(pyRDFURN *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFURN_relative_name(pyRDFURN *self, PyObject *args, PyObject *kwds);
/*****************************************************
             Implementation
******************************************************/

static PyMethodDef MapDriver_methods[] = {
     {"set_property",(PyCFunction)pyMapDriver_set_property, METH_VARARGS|METH_KEYWORDS, "void * MapDriver.set_property(char * attribute ,RDFValue value );\n\n\n"},
     {"finish",(PyCFunction)pyMapDriver_finish, METH_VARARGS|METH_KEYWORDS, "AFFObject MapDriver.finish();\n\n\n"},
     {"seek",(PyCFunction)pyMapDriver_seek, METH_VARARGS|METH_KEYWORDS, "uint64_t  MapDriver.seek(uint64_t  offset ,uint64_t  whence );\n\n\n"},
     {"read",(PyCFunction)pyMapDriver_read, METH_VARARGS|METH_KEYWORDS, "uint64_t  MapDriver.read(OUT char * buffer, unsigned long int length);\n\n\n"},
     {"write",(PyCFunction)pyMapDriver_write, METH_VARARGS|METH_KEYWORDS, "uint64_t  MapDriver.write(char * buffer, unsigned long int length);\n\n\n"},
     {"tell",(PyCFunction)pyMapDriver_tell, METH_VARARGS|METH_KEYWORDS, "uint64_t  MapDriver.tell();\n\n\n"},
     {"get_data",(PyCFunction)pyMapDriver_get_data, METH_VARARGS|METH_KEYWORDS, "char * MapDriver.get_data();\n\n\n"},
     {"truncate",(PyCFunction)pyMapDriver_truncate, METH_VARARGS|METH_KEYWORDS, "uint64_t  MapDriver.truncate(uint64_t  offset );\n\n\n"},
     {"close",(PyCFunction)pyMapDriver_close, METH_VARARGS|METH_KEYWORDS, "void * MapDriver.close();\n\n\n"},
     {"del",(PyCFunction)pyMapDriver_del, METH_VARARGS|METH_KEYWORDS, "void * MapDriver.del(uint64_t  target_pos );\n\n\n Deletes the point at the specified file offset\n"},
     {"add",(PyCFunction)pyMapDriver_add, METH_VARARGS|METH_KEYWORDS, "void * MapDriver.add(uint64_t  image_offset ,uint64_t  target_offset ,char * target );\n\n\n Adds a new point ot the file offset table\n"},
     {"save_map",(PyCFunction)pyMapDriver_save_map, METH_VARARGS|METH_KEYWORDS, "void * MapDriver.save_map();\n\n\n"},
     {NULL}  /* Sentinel */
};
static void
MapDriver_dealloc(pyMapDriver *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyMapDriver_init(pyMapDriver *self, PyObject *args, PyObject *kwds) {
Gen_wrapper *urn;
char mode=0; char *str_mode = "\x0";
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|", kwlist, &urn,&str_mode))
 goto error;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

self->base = CONSTRUCT(MapDriver, AFFObject, Con, NULL, urn->base, mode);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class MapDriver");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *MapDriver_getattr(pyMapDriver *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("readptr");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("data");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("number_of_points");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("target_period");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("image_period");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("blocksize");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("stored");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("map_urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=MapDriver_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "urn")) {
    PyObject *py_result;
    Gen_wrapper *urn;


    {
       Object returned_object = (Object)(((AFFObject)self->base)->urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       urn = new_class_wrapper(returned_object);
       if(!urn) goto error;
    }
talloc_increase_ref_count(urn->base);

    py_result = (PyObject *)urn;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((AFFObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "readptr")) {
    PyObject *py_result;
    uint64_t  readptr;


    readptr = (((FileLikeObject)self->base)->readptr);

    py_result = PyLong_FromLong(readptr);

    return py_result;
};
if(!strcmp(name, "size")) {
    PyObject *py_result;
    Gen_wrapper *size;


    {
       Object returned_object = (Object)(((FileLikeObject)self->base)->size);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       size = new_class_wrapper(returned_object);
       if(!size) goto error;
    }
talloc_increase_ref_count(size->base);

    py_result = (PyObject *)size;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((FileLikeObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "data")) {
    PyObject *py_result;
    char * data;


    data = (((FileLikeObject)self->base)->data);

    py_result = PyString_FromStringAndSize((char *)data, strlen(data));

    return py_result;
};
if(!strcmp(name, "number_of_points")) {
    PyObject *py_result;
    uint64_t  number_of_points;


    number_of_points = (((MapDriver)self->base)->number_of_points);

    py_result = PyLong_FromLong(number_of_points);

    return py_result;
};
if(!strcmp(name, "target_period")) {
    PyObject *py_result;
    Gen_wrapper *target_period;


    {
       Object returned_object = (Object)(((MapDriver)self->base)->target_period);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       target_period = new_class_wrapper(returned_object);
       if(!target_period) goto error;
    }
talloc_increase_ref_count(target_period->base);

    py_result = (PyObject *)target_period;

    return py_result;
};
if(!strcmp(name, "image_period")) {
    PyObject *py_result;
    Gen_wrapper *image_period;


    {
       Object returned_object = (Object)(((MapDriver)self->base)->image_period);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       image_period = new_class_wrapper(returned_object);
       if(!image_period) goto error;
    }
talloc_increase_ref_count(image_period->base);

    py_result = (PyObject *)image_period;

    return py_result;
};
if(!strcmp(name, "blocksize")) {
    PyObject *py_result;
    Gen_wrapper *blocksize;


    {
       Object returned_object = (Object)(((MapDriver)self->base)->blocksize);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       blocksize = new_class_wrapper(returned_object);
       if(!blocksize) goto error;
    }
talloc_increase_ref_count(blocksize->base);

    py_result = (PyObject *)blocksize;

    return py_result;
};
if(!strcmp(name, "stored")) {
    PyObject *py_result;
    Gen_wrapper *stored;


    {
       Object returned_object = (Object)(((MapDriver)self->base)->stored);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       stored = new_class_wrapper(returned_object);
       if(!stored) goto error;
    }
talloc_increase_ref_count(stored->base);

    py_result = (PyObject *)stored;

    return py_result;
};
if(!strcmp(name, "map_urn")) {
    PyObject *py_result;
    Gen_wrapper *map_urn;


    {
       Object returned_object = (Object)(((MapDriver)self->base)->map_urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       map_urn = new_class_wrapper(returned_object);
       if(!map_urn) goto error;
    }
talloc_increase_ref_count(map_urn->base);

    py_result = (PyObject *)map_urn;

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
void * MapDriver.set_property(char * attribute ,RDFValue value );
********************************************************/

static PyObject *pyMapDriver_set_property(pyMapDriver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * attribute;
Gen_wrapper *value;
static char *kwlist[] = {"attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "sO|", kwlist, &attribute,&value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "MapDriver object no longer valid");
// Precall preparations

// Make the call
((AFFObject)self->base)->set_property(((AFFObject)self->base), attribute, value->base);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
AFFObject MapDriver.finish();
********************************************************/

static PyObject *pyMapDriver_finish(pyMapDriver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "MapDriver object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((AFFObject)self->base)->finish(((AFFObject)self->base));

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations
self->base = NULL;

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    self->base = NULL;
return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  MapDriver.seek(uint64_t  offset ,uint64_t  whence );
********************************************************/

static PyObject *pyMapDriver_seek(pyMapDriver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
uint64_t  whence;
static char *kwlist[] = {"offset","whence", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "KK|", kwlist, &offset,&whence))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "MapDriver object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->seek(((FileLikeObject)self->base), offset, whence);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  MapDriver.read(OUT char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyMapDriver_read(pyMapDriver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
PyObject *tmp_buffer;
static char *kwlist[] = {"length", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "l|", kwlist, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "MapDriver object no longer valid");
// Precall preparations
tmp_buffer = PyString_FromStringAndSize(NULL, length);
PyString_AsStringAndSize(tmp_buffer, &buffer, (Py_ssize_t *)&length);

// Make the call
func_return = ((FileLikeObject)self->base)->read(((FileLikeObject)self->base), (OUT char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
returned_result = PyList_New(0);
py_result = PyLong_FromLong(func_return);
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
 _PyString_Resize(&tmp_buffer, func_return); 
py_result = tmp_buffer;
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  MapDriver.write(char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyMapDriver_write(pyMapDriver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"buffer", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &buffer, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "MapDriver object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->write(((FileLikeObject)self->base), (char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  MapDriver.tell();
********************************************************/

static PyObject *pyMapDriver_tell(pyMapDriver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "MapDriver object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->tell(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
char * MapDriver.get_data();
********************************************************/

static PyObject *pyMapDriver_get_data(pyMapDriver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "MapDriver object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->get_data(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  MapDriver.truncate(uint64_t  offset );
********************************************************/

static PyObject *pyMapDriver_truncate(pyMapDriver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
static char *kwlist[] = {"offset", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "K|", kwlist, &offset))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "MapDriver object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->truncate(((FileLikeObject)self->base), offset);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * MapDriver.close();
********************************************************/

static PyObject *pyMapDriver_close(pyMapDriver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "MapDriver object no longer valid");
// Precall preparations

// Make the call
((FileLikeObject)self->base)->close(((FileLikeObject)self->base));

// Postcall preparations
self->base = NULL;

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
void * MapDriver.del(uint64_t  target_pos );
********************************************************/

static PyObject *pyMapDriver_del(pyMapDriver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  target_pos;
static char *kwlist[] = {"target_pos", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "K|", kwlist, &target_pos))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "MapDriver object no longer valid");
// Precall preparations

// Make the call
((MapDriver)self->base)->del(((MapDriver)self->base), target_pos);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * MapDriver.add(uint64_t  image_offset ,uint64_t  target_offset ,char * target );
********************************************************/

static PyObject *pyMapDriver_add(pyMapDriver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  image_offset;
uint64_t  target_offset;
char * target;
static char *kwlist[] = {"image_offset","target_offset","target", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "KKs|", kwlist, &image_offset,&target_offset,&target))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "MapDriver object no longer valid");
// Precall preparations

// Make the call
((MapDriver)self->base)->add(((MapDriver)self->base), image_offset, target_offset, target);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * MapDriver.save_map();
********************************************************/

static PyObject *pyMapDriver_save_map(pyMapDriver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "MapDriver object no longer valid");
// Precall preparations

// Make the call
((MapDriver)self->base)->save_map(((MapDriver)self->base));

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

};


static PyTypeObject MapDriver_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.MapDriver",               /* tp_name */
    sizeof(pyMapDriver),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)MapDriver_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)MapDriver_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "MapDriver: ",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    MapDriver_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyMapDriver_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef AES256Password_methods[] = {
     {"parse",(PyCFunction)pyAES256Password_parse, METH_VARARGS|METH_KEYWORDS, "uint64_t  AES256Password.parse(char * serialised_form );\n\n\n"},
     {"encode",(PyCFunction)pyAES256Password_encode, METH_VARARGS|METH_KEYWORDS, "TDB_DATA * AES256Password.encode();\n\n\n"},
     {"decode",(PyCFunction)pyAES256Password_decode, METH_VARARGS|METH_KEYWORDS, "uint64_t  AES256Password.decode(char * data, int length);\n\n\n"},
     {"serialise",(PyCFunction)pyAES256Password_serialise, METH_VARARGS|METH_KEYWORDS, "char * AES256Password.serialise();\n\n\n"},
     {"encrypt",(PyCFunction)pyAES256Password_encrypt, METH_VARARGS|METH_KEYWORDS, "uint64_t  AES256Password.encrypt(unsigned char * inbuff, int inlen,OUT unsigned char * outbuf, int outlen,uint64_t  chunk_number );\n\n\n"},
     {"decrypt",(PyCFunction)pyAES256Password_decrypt, METH_VARARGS|METH_KEYWORDS, "uint64_t  AES256Password.decrypt(unsigned char * inbuff, int inlen,OUT unsigned char * outbuf, int outlen,uint64_t  chunk_number );\n\n\n"},
     {"set",(PyCFunction)pyAES256Password_set, METH_VARARGS|METH_KEYWORDS, "RDFValue AES256Password.set(char * passphrase );\n\n\n Set the password for this object. Should only be called once\n before using.\n"},
     {"fetch_password_cb",(PyCFunction)pyAES256Password_fetch_password_cb, METH_VARARGS|METH_KEYWORDS, "uint64_t  AES256Password.fetch_password_cb();\n\n\n This callback can be overridden to fetch password to decode the\n IV from. By default, we look in the AFF4_VOLATILE_PASSPHRASE\n environment variable.\n"},
     {NULL}  /* Sentinel */
};
static void
AES256Password_dealloc(pyAES256Password *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyAES256Password_init(pyAES256Password *self, PyObject *args, PyObject *kwds) {

self->base = CONSTRUCT(AES256Password, RDFValue, Con, NULL);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class AES256Password");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *AES256Password_getattr(pyAES256Password *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("dataType");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("id");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("blocksize");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=AES256Password_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "dataType")) {
    PyObject *py_result;
    char * dataType;


    dataType = (((RDFValue)self->base)->dataType);

    py_result = PyString_FromStringAndSize((char *)dataType, strlen(dataType));

    return py_result;
};
if(!strcmp(name, "id")) {
    PyObject *py_result;
    uint64_t  id;


    id = (((RDFValue)self->base)->id);

    py_result = PyLong_FromLong(id);

    return py_result;
};
if(!strcmp(name, "blocksize")) {
    PyObject *py_result;
    uint64_t  blocksize;


    blocksize = (((AFF4Cipher)self->base)->blocksize);

    py_result = PyLong_FromLong(blocksize);

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
uint64_t  AES256Password.parse(char * serialised_form );
********************************************************/

static PyObject *pyAES256Password_parse(pyAES256Password *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char * serialised_form;
static char *kwlist[] = {"serialised_form", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &serialised_form))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "AES256Password object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->parse(((RDFValue)self->base), serialised_form);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA * AES256Password.encode();
********************************************************/

static PyObject *pyAES256Password_encode(pyAES256Password *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
TDB_DATA * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "AES256Password object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->encode(((RDFValue)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return->dptr, func_return->dsize);
talloc_free(func_return);returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  AES256Password.decode(char * data, int length);
********************************************************/

static PyObject *pyAES256Password_decode(pyAES256Password *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *data=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"data", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &data, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "AES256Password object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->decode(((RDFValue)self->base), (char *)data, (int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
char * AES256Password.serialise();
********************************************************/

static PyObject *pyAES256Password_serialise(pyAES256Password *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "AES256Password object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->serialise(((RDFValue)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  AES256Password.encrypt(unsigned char * inbuff, int inlen,OUT unsigned char * outbuf, int outlen,uint64_t  chunk_number );
********************************************************/

static PyObject *pyAES256Password_encrypt(pyAES256Password *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *inbuff=""; Py_ssize_t inlen=strlen("");
char *outbuf=""; Py_ssize_t outlen=strlen("");
PyObject *tmp_outbuf;
uint64_t  chunk_number;
static char *kwlist[] = {"inbuff","outlen","chunk_number", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#lK|", kwlist, &inbuff, &inlen,&outlen,&chunk_number))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "AES256Password object no longer valid");
// Precall preparations
tmp_outbuf = PyString_FromStringAndSize(NULL, outlen);
PyString_AsStringAndSize(tmp_outbuf, &outbuf, (Py_ssize_t *)&outlen);

// Make the call
func_return = ((AFF4Cipher)self->base)->encrypt(((AFF4Cipher)self->base), (unsigned char *)inbuff, (int)inlen, (OUT unsigned char *)outbuf, (int)outlen, chunk_number);

// Postcall preparations

// prepare results
returned_result = PyList_New(0);
py_result = PyLong_FromLong(func_return);
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
 _PyString_Resize(&tmp_outbuf, func_return); 
py_result = tmp_outbuf;
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  AES256Password.decrypt(unsigned char * inbuff, int inlen,OUT unsigned char * outbuf, int outlen,uint64_t  chunk_number );
********************************************************/

static PyObject *pyAES256Password_decrypt(pyAES256Password *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *inbuff=""; Py_ssize_t inlen=strlen("");
char *outbuf=""; Py_ssize_t outlen=strlen("");
PyObject *tmp_outbuf;
uint64_t  chunk_number;
static char *kwlist[] = {"inbuff","outlen","chunk_number", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#lK|", kwlist, &inbuff, &inlen,&outlen,&chunk_number))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "AES256Password object no longer valid");
// Precall preparations
tmp_outbuf = PyString_FromStringAndSize(NULL, outlen);
PyString_AsStringAndSize(tmp_outbuf, &outbuf, (Py_ssize_t *)&outlen);

// Make the call
func_return = ((AFF4Cipher)self->base)->decrypt(((AFF4Cipher)self->base), (unsigned char *)inbuff, (int)inlen, (OUT unsigned char *)outbuf, (int)outlen, chunk_number);

// Postcall preparations

// prepare results
returned_result = PyList_New(0);
py_result = PyLong_FromLong(func_return);
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
 _PyString_Resize(&tmp_outbuf, func_return); 
py_result = tmp_outbuf;
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
RDFValue AES256Password.set(char * passphrase );
********************************************************/

static PyObject *pyAES256Password_set(pyAES256Password *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
char * passphrase;
static char *kwlist[] = {"passphrase", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &passphrase))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "AES256Password object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((AES256Password)self->base)->set(((AES256Password)self->base), passphrase);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFValue: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }
talloc_increase_ref_count(func_return->base);

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  AES256Password.fetch_password_cb();
********************************************************/

static PyObject *pyAES256Password_fetch_password_cb(pyAES256Password *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "AES256Password object no longer valid");
// Precall preparations

// Make the call
func_return = ((AES256Password)self->base)->fetch_password_cb(((AES256Password)self->base));

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

};


static PyTypeObject AES256Password_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.AES256Password",               /* tp_name */
    sizeof(pyAES256Password),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)AES256Password_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)AES256Password_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "AES256Password:  This cipher uses AES256. The session key is derived from a\n    password using the PBKDF2 algorithm.\n",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    AES256Password_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyAES256Password_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef XSDString_methods[] = {
     {"parse",(PyCFunction)pyXSDString_parse, METH_VARARGS|METH_KEYWORDS, "uint64_t  XSDString.parse(char * serialised_form );\n\n\n"},
     {"encode",(PyCFunction)pyXSDString_encode, METH_VARARGS|METH_KEYWORDS, "TDB_DATA * XSDString.encode();\n\n\n"},
     {"decode",(PyCFunction)pyXSDString_decode, METH_VARARGS|METH_KEYWORDS, "uint64_t  XSDString.decode(char * data, int length);\n\n\n"},
     {"serialise",(PyCFunction)pyXSDString_serialise, METH_VARARGS|METH_KEYWORDS, "char * XSDString.serialise();\n\n\n"},
     {"set",(PyCFunction)pyXSDString_set, METH_VARARGS|METH_KEYWORDS, "RDFValue XSDString.set(char * string, int length);\n\n\n"},
     {"get",(PyCFunction)pyXSDString_get, METH_VARARGS|METH_KEYWORDS, "char * XSDString.get();\n\n\n"},
     {NULL}  /* Sentinel */
};
static void
XSDString_dealloc(pyXSDString *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyXSDString_init(pyXSDString *self, PyObject *args, PyObject *kwds) {

self->base = CONSTRUCT(XSDString, RDFValue, Con, NULL);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class XSDString");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *XSDString_getattr(pyXSDString *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("dataType");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("id");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("value");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("length");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=XSDString_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "dataType")) {
    PyObject *py_result;
    char * dataType;


    dataType = (((RDFValue)self->base)->dataType);

    py_result = PyString_FromStringAndSize((char *)dataType, strlen(dataType));

    return py_result;
};
if(!strcmp(name, "id")) {
    PyObject *py_result;
    uint64_t  id;


    id = (((RDFValue)self->base)->id);

    py_result = PyLong_FromLong(id);

    return py_result;
};
if(!strcmp(name, "value")) {
    PyObject *py_result;
    char * value;


    value = (((XSDString)self->base)->value);

    py_result = PyString_FromStringAndSize((char *)value, strlen(value));

    return py_result;
};
if(!strcmp(name, "length")) {
    PyObject *py_result;
    uint64_t  length;


    length = (((XSDString)self->base)->length);

    py_result = PyLong_FromLong(length);

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDString.parse(char * serialised_form );
********************************************************/

static PyObject *pyXSDString_parse(pyXSDString *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char * serialised_form;
static char *kwlist[] = {"serialised_form", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &serialised_form))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDString object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->parse(((RDFValue)self->base), serialised_form);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA * XSDString.encode();
********************************************************/

static PyObject *pyXSDString_encode(pyXSDString *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
TDB_DATA * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDString object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->encode(((RDFValue)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return->dptr, func_return->dsize);
talloc_free(func_return);returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDString.decode(char * data, int length);
********************************************************/

static PyObject *pyXSDString_decode(pyXSDString *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *data=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"data", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &data, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDString object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->decode(((RDFValue)self->base), (char *)data, (int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
char * XSDString.serialise();
********************************************************/

static PyObject *pyXSDString_serialise(pyXSDString *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDString object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->serialise(((RDFValue)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
RDFValue XSDString.set(char * string, int length);
********************************************************/

static PyObject *pyXSDString_set(pyXSDString *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
char *string=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"string", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &string, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDString object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((XSDString)self->base)->set(((XSDString)self->base), (char *)string, (int)length);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFValue: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }
talloc_increase_ref_count(func_return->base);

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
char * XSDString.get();
********************************************************/

static PyObject *pyXSDString_get(pyXSDString *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDString object no longer valid");
// Precall preparations

// Make the call
func_return = ((XSDString)self->base)->get(((XSDString)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
returned_result = py_result;
return returned_result;

};


static PyTypeObject XSDString_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.XSDString",               /* tp_name */
    sizeof(pyXSDString),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)XSDString_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)XSDString_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "XSDString:  A literal string ",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    XSDString_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyXSDString_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef AFFObject_methods[] = {
     {"set_property",(PyCFunction)pyAFFObject_set_property, METH_VARARGS|METH_KEYWORDS, "void * AFFObject.set_property(char * attribute ,RDFValue value );\n\n\n This is called to set properties on the object "},
     {"finish",(PyCFunction)pyAFFObject_finish, METH_VARARGS|METH_KEYWORDS, "AFFObject AFFObject.finish();\n\n\n Finally the object may be ready for use. We return the ready\n\t object or NULL if something went wrong.\n     "},
     {NULL}  /* Sentinel */
};
static void
AFFObject_dealloc(pyAFFObject *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyAFFObject_init(pyAFFObject *self, PyObject *args, PyObject *kwds) {
Gen_wrapper *urn;
char mode=0; char *str_mode = "\x0";
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|", kwlist, &urn,&str_mode))
 goto error;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

self->base = CONSTRUCT(AFFObject, AFFObject, Con, NULL, urn->base, mode);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class AFFObject");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *AFFObject_getattr(pyAFFObject *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=AFFObject_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "urn")) {
    PyObject *py_result;
    Gen_wrapper *urn;


    {
       Object returned_object = (Object)(((AFFObject)self->base)->urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       urn = new_class_wrapper(returned_object);
       if(!urn) goto error;
    }
talloc_increase_ref_count(urn->base);

    py_result = (PyObject *)urn;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((AFFObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
void * AFFObject.set_property(char * attribute ,RDFValue value );
********************************************************/

static PyObject *pyAFFObject_set_property(pyAFFObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * attribute;
Gen_wrapper *value;
static char *kwlist[] = {"attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "sO|", kwlist, &attribute,&value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "AFFObject object no longer valid");
// Precall preparations

// Make the call
((AFFObject)self->base)->set_property(((AFFObject)self->base), attribute, value->base);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
AFFObject AFFObject.finish();
********************************************************/

static PyObject *pyAFFObject_finish(pyAFFObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "AFFObject object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((AFFObject)self->base)->finish(((AFFObject)self->base));

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations
self->base = NULL;

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    self->base = NULL;
return NULL;

};


static PyTypeObject AFFObject_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.AFFObject",               /* tp_name */
    sizeof(pyAFFObject),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)AFFObject_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)AFFObject_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "AFFObject:  All AFF Objects inherit from this one. The URI must be set to\n    represent the globally unique URI of this object. ",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    AFFObject_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyAFFObject_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef Image_methods[] = {
     {"set_property",(PyCFunction)pyImage_set_property, METH_VARARGS|METH_KEYWORDS, "void * Image.set_property(char * attribute ,RDFValue value );\n\n\n"},
     {"finish",(PyCFunction)pyImage_finish, METH_VARARGS|METH_KEYWORDS, "AFFObject Image.finish();\n\n\n"},
     {"seek",(PyCFunction)pyImage_seek, METH_VARARGS|METH_KEYWORDS, "uint64_t  Image.seek(uint64_t  offset ,uint64_t  whence );\n\n\n"},
     {"read",(PyCFunction)pyImage_read, METH_VARARGS|METH_KEYWORDS, "uint64_t  Image.read(OUT char * buffer, unsigned long int length);\n\n\n"},
     {"write",(PyCFunction)pyImage_write, METH_VARARGS|METH_KEYWORDS, "uint64_t  Image.write(char * buffer, unsigned long int length);\n\n\n"},
     {"tell",(PyCFunction)pyImage_tell, METH_VARARGS|METH_KEYWORDS, "uint64_t  Image.tell();\n\n\n"},
     {"get_data",(PyCFunction)pyImage_get_data, METH_VARARGS|METH_KEYWORDS, "char * Image.get_data();\n\n\n"},
     {"truncate",(PyCFunction)pyImage_truncate, METH_VARARGS|METH_KEYWORDS, "uint64_t  Image.truncate(uint64_t  offset );\n\n\n"},
     {"close",(PyCFunction)pyImage_close, METH_VARARGS|METH_KEYWORDS, "void * Image.close();\n\n\n"},
     {NULL}  /* Sentinel */
};
static void
Image_dealloc(pyImage *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyImage_init(pyImage *self, PyObject *args, PyObject *kwds) {
Gen_wrapper *urn;
char mode=0; char *str_mode = "\x0";
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|", kwlist, &urn,&str_mode))
 goto error;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

self->base = CONSTRUCT(Image, AFFObject, Con, NULL, urn->base, mode);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class Image");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *Image_getattr(pyImage *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("readptr");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("data");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("stored");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("bevy_urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("current");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("chunk_size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("compression");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("chunks_in_segment");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("bevy_size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("segment_count");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=Image_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "urn")) {
    PyObject *py_result;
    Gen_wrapper *urn;


    {
       Object returned_object = (Object)(((AFFObject)self->base)->urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       urn = new_class_wrapper(returned_object);
       if(!urn) goto error;
    }
talloc_increase_ref_count(urn->base);

    py_result = (PyObject *)urn;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((AFFObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "readptr")) {
    PyObject *py_result;
    uint64_t  readptr;


    readptr = (((FileLikeObject)self->base)->readptr);

    py_result = PyLong_FromLong(readptr);

    return py_result;
};
if(!strcmp(name, "size")) {
    PyObject *py_result;
    Gen_wrapper *size;


    {
       Object returned_object = (Object)(((FileLikeObject)self->base)->size);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       size = new_class_wrapper(returned_object);
       if(!size) goto error;
    }
talloc_increase_ref_count(size->base);

    py_result = (PyObject *)size;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((FileLikeObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "data")) {
    PyObject *py_result;
    char * data;


    data = (((FileLikeObject)self->base)->data);

    py_result = PyString_FromStringAndSize((char *)data, strlen(data));

    return py_result;
};
if(!strcmp(name, "stored")) {
    PyObject *py_result;
    Gen_wrapper *stored;


    {
       Object returned_object = (Object)(((Image)self->base)->stored);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       stored = new_class_wrapper(returned_object);
       if(!stored) goto error;
    }
talloc_increase_ref_count(stored->base);

    py_result = (PyObject *)stored;

    return py_result;
};
if(!strcmp(name, "bevy_urn")) {
    PyObject *py_result;
    Gen_wrapper *bevy_urn;


    {
       Object returned_object = (Object)(((Image)self->base)->bevy_urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       bevy_urn = new_class_wrapper(returned_object);
       if(!bevy_urn) goto error;
    }
talloc_increase_ref_count(bevy_urn->base);

    py_result = (PyObject *)bevy_urn;

    return py_result;
};
if(!strcmp(name, "current")) {
    PyObject *py_result;
    Gen_wrapper *current;


    {
       Object returned_object = (Object)(((Image)self->base)->current);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object ImageWorker: %s", __error_str);
         ClearError();
         goto error;
       };

       current = new_class_wrapper(returned_object);
       if(!current) goto error;
    }
talloc_increase_ref_count(current->base);

    py_result = (PyObject *)current;

    return py_result;
};
if(!strcmp(name, "chunk_size")) {
    PyObject *py_result;
    Gen_wrapper *chunk_size;


    {
       Object returned_object = (Object)(((Image)self->base)->chunk_size);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       chunk_size = new_class_wrapper(returned_object);
       if(!chunk_size) goto error;
    }
talloc_increase_ref_count(chunk_size->base);

    py_result = (PyObject *)chunk_size;

    return py_result;
};
if(!strcmp(name, "compression")) {
    PyObject *py_result;
    Gen_wrapper *compression;


    {
       Object returned_object = (Object)(((Image)self->base)->compression);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       compression = new_class_wrapper(returned_object);
       if(!compression) goto error;
    }
talloc_increase_ref_count(compression->base);

    py_result = (PyObject *)compression;

    return py_result;
};
if(!strcmp(name, "chunks_in_segment")) {
    PyObject *py_result;
    Gen_wrapper *chunks_in_segment;


    {
       Object returned_object = (Object)(((Image)self->base)->chunks_in_segment);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       chunks_in_segment = new_class_wrapper(returned_object);
       if(!chunks_in_segment) goto error;
    }
talloc_increase_ref_count(chunks_in_segment->base);

    py_result = (PyObject *)chunks_in_segment;

    return py_result;
};
if(!strcmp(name, "bevy_size")) {
    PyObject *py_result;
    uint64_t  bevy_size;


    bevy_size = (((Image)self->base)->bevy_size);

    py_result = PyLong_FromLong(bevy_size);

    return py_result;
};
if(!strcmp(name, "segment_count")) {
    PyObject *py_result;
    uint64_t  segment_count;


    segment_count = (((Image)self->base)->segment_count);

    py_result = PyLong_FromLong(segment_count);

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
void * Image.set_property(char * attribute ,RDFValue value );
********************************************************/

static PyObject *pyImage_set_property(pyImage *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * attribute;
Gen_wrapper *value;
static char *kwlist[] = {"attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "sO|", kwlist, &attribute,&value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Image object no longer valid");
// Precall preparations

// Make the call
((AFFObject)self->base)->set_property(((AFFObject)self->base), attribute, value->base);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
AFFObject Image.finish();
********************************************************/

static PyObject *pyImage_finish(pyImage *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Image object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((AFFObject)self->base)->finish(((AFFObject)self->base));

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations
self->base = NULL;

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    self->base = NULL;
return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Image.seek(uint64_t  offset ,uint64_t  whence );
********************************************************/

static PyObject *pyImage_seek(pyImage *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
uint64_t  whence;
static char *kwlist[] = {"offset","whence", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "KK|", kwlist, &offset,&whence))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Image object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->seek(((FileLikeObject)self->base), offset, whence);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Image.read(OUT char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyImage_read(pyImage *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
PyObject *tmp_buffer;
static char *kwlist[] = {"length", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "l|", kwlist, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Image object no longer valid");
// Precall preparations
tmp_buffer = PyString_FromStringAndSize(NULL, length);
PyString_AsStringAndSize(tmp_buffer, &buffer, (Py_ssize_t *)&length);

// Make the call
func_return = ((FileLikeObject)self->base)->read(((FileLikeObject)self->base), (OUT char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
returned_result = PyList_New(0);
py_result = PyLong_FromLong(func_return);
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
 _PyString_Resize(&tmp_buffer, func_return); 
py_result = tmp_buffer;
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Image.write(char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyImage_write(pyImage *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"buffer", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &buffer, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Image object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->write(((FileLikeObject)self->base), (char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Image.tell();
********************************************************/

static PyObject *pyImage_tell(pyImage *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Image object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->tell(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
char * Image.get_data();
********************************************************/

static PyObject *pyImage_get_data(pyImage *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Image object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->get_data(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Image.truncate(uint64_t  offset );
********************************************************/

static PyObject *pyImage_truncate(pyImage *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
static char *kwlist[] = {"offset", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "K|", kwlist, &offset))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Image object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->truncate(((FileLikeObject)self->base), offset);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * Image.close();
********************************************************/

static PyObject *pyImage_close(pyImage *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Image object no longer valid");
// Precall preparations

// Make the call
((FileLikeObject)self->base)->close(((FileLikeObject)self->base));

// Postcall preparations
self->base = NULL;

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

};


static PyTypeObject Image_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.Image",               /* tp_name */
    sizeof(pyImage),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)Image_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)Image_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "Image:  The Image Stream represents an Image in chunks ",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Image_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyImage_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef Encrypted_methods[] = {
     {"set_property",(PyCFunction)pyEncrypted_set_property, METH_VARARGS|METH_KEYWORDS, "void * Encrypted.set_property(char * attribute ,RDFValue value );\n\n\n"},
     {"finish",(PyCFunction)pyEncrypted_finish, METH_VARARGS|METH_KEYWORDS, "AFFObject Encrypted.finish();\n\n\n"},
     {"seek",(PyCFunction)pyEncrypted_seek, METH_VARARGS|METH_KEYWORDS, "uint64_t  Encrypted.seek(uint64_t  offset ,uint64_t  whence );\n\n\n"},
     {"read",(PyCFunction)pyEncrypted_read, METH_VARARGS|METH_KEYWORDS, "uint64_t  Encrypted.read(OUT char * buffer, unsigned long int length);\n\n\n"},
     {"write",(PyCFunction)pyEncrypted_write, METH_VARARGS|METH_KEYWORDS, "uint64_t  Encrypted.write(char * buffer, unsigned long int length);\n\n\n"},
     {"tell",(PyCFunction)pyEncrypted_tell, METH_VARARGS|METH_KEYWORDS, "uint64_t  Encrypted.tell();\n\n\n"},
     {"get_data",(PyCFunction)pyEncrypted_get_data, METH_VARARGS|METH_KEYWORDS, "char * Encrypted.get_data();\n\n\n"},
     {"truncate",(PyCFunction)pyEncrypted_truncate, METH_VARARGS|METH_KEYWORDS, "uint64_t  Encrypted.truncate(uint64_t  offset );\n\n\n"},
     {"close",(PyCFunction)pyEncrypted_close, METH_VARARGS|METH_KEYWORDS, "void * Encrypted.close();\n\n\n"},
     {NULL}  /* Sentinel */
};
static void
Encrypted_dealloc(pyEncrypted *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyEncrypted_init(pyEncrypted *self, PyObject *args, PyObject *kwds) {
Gen_wrapper *urn;
char mode=0; char *str_mode = "\x0";
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|", kwlist, &urn,&str_mode))
 goto error;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

self->base = CONSTRUCT(Encrypted, AFFObject, Con, NULL, urn->base, mode);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class Encrypted");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *Encrypted_getattr(pyEncrypted *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("readptr");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("data");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("cipher");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("backing_store");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("stored");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("chunk_size");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=Encrypted_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "urn")) {
    PyObject *py_result;
    Gen_wrapper *urn;


    {
       Object returned_object = (Object)(((AFFObject)self->base)->urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       urn = new_class_wrapper(returned_object);
       if(!urn) goto error;
    }
talloc_increase_ref_count(urn->base);

    py_result = (PyObject *)urn;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((AFFObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "readptr")) {
    PyObject *py_result;
    uint64_t  readptr;


    readptr = (((FileLikeObject)self->base)->readptr);

    py_result = PyLong_FromLong(readptr);

    return py_result;
};
if(!strcmp(name, "size")) {
    PyObject *py_result;
    Gen_wrapper *size;


    {
       Object returned_object = (Object)(((FileLikeObject)self->base)->size);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       size = new_class_wrapper(returned_object);
       if(!size) goto error;
    }
talloc_increase_ref_count(size->base);

    py_result = (PyObject *)size;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((FileLikeObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "data")) {
    PyObject *py_result;
    char * data;


    data = (((FileLikeObject)self->base)->data);

    py_result = PyString_FromStringAndSize((char *)data, strlen(data));

    return py_result;
};
if(!strcmp(name, "cipher")) {
    PyObject *py_result;
    Gen_wrapper *cipher;


    {
       Object returned_object = (Object)(((Encrypted)self->base)->cipher);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFF4Cipher: %s", __error_str);
         ClearError();
         goto error;
       };

       cipher = new_class_wrapper(returned_object);
       if(!cipher) goto error;
    }
talloc_increase_ref_count(cipher->base);

    py_result = (PyObject *)cipher;

    return py_result;
};
if(!strcmp(name, "backing_store")) {
    PyObject *py_result;
    Gen_wrapper *backing_store;


    {
       Object returned_object = (Object)(((Encrypted)self->base)->backing_store);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       backing_store = new_class_wrapper(returned_object);
       if(!backing_store) goto error;
    }
talloc_increase_ref_count(backing_store->base);

    py_result = (PyObject *)backing_store;

    return py_result;
};
if(!strcmp(name, "stored")) {
    PyObject *py_result;
    Gen_wrapper *stored;


    {
       Object returned_object = (Object)(((Encrypted)self->base)->stored);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       stored = new_class_wrapper(returned_object);
       if(!stored) goto error;
    }
talloc_increase_ref_count(stored->base);

    py_result = (PyObject *)stored;

    return py_result;
};
if(!strcmp(name, "chunk_size")) {
    PyObject *py_result;
    Gen_wrapper *chunk_size;


    {
       Object returned_object = (Object)(((Encrypted)self->base)->chunk_size);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       chunk_size = new_class_wrapper(returned_object);
       if(!chunk_size) goto error;
    }
talloc_increase_ref_count(chunk_size->base);

    py_result = (PyObject *)chunk_size;

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
void * Encrypted.set_property(char * attribute ,RDFValue value );
********************************************************/

static PyObject *pyEncrypted_set_property(pyEncrypted *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * attribute;
Gen_wrapper *value;
static char *kwlist[] = {"attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "sO|", kwlist, &attribute,&value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Encrypted object no longer valid");
// Precall preparations

// Make the call
((AFFObject)self->base)->set_property(((AFFObject)self->base), attribute, value->base);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
AFFObject Encrypted.finish();
********************************************************/

static PyObject *pyEncrypted_finish(pyEncrypted *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Encrypted object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((AFFObject)self->base)->finish(((AFFObject)self->base));

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations
self->base = NULL;

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    self->base = NULL;
return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Encrypted.seek(uint64_t  offset ,uint64_t  whence );
********************************************************/

static PyObject *pyEncrypted_seek(pyEncrypted *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
uint64_t  whence;
static char *kwlist[] = {"offset","whence", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "KK|", kwlist, &offset,&whence))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Encrypted object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->seek(((FileLikeObject)self->base), offset, whence);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Encrypted.read(OUT char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyEncrypted_read(pyEncrypted *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
PyObject *tmp_buffer;
static char *kwlist[] = {"length", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "l|", kwlist, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Encrypted object no longer valid");
// Precall preparations
tmp_buffer = PyString_FromStringAndSize(NULL, length);
PyString_AsStringAndSize(tmp_buffer, &buffer, (Py_ssize_t *)&length);

// Make the call
func_return = ((FileLikeObject)self->base)->read(((FileLikeObject)self->base), (OUT char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
returned_result = PyList_New(0);
py_result = PyLong_FromLong(func_return);
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
 _PyString_Resize(&tmp_buffer, func_return); 
py_result = tmp_buffer;
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Encrypted.write(char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyEncrypted_write(pyEncrypted *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"buffer", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &buffer, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Encrypted object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->write(((FileLikeObject)self->base), (char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Encrypted.tell();
********************************************************/

static PyObject *pyEncrypted_tell(pyEncrypted *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Encrypted object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->tell(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
char * Encrypted.get_data();
********************************************************/

static PyObject *pyEncrypted_get_data(pyEncrypted *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Encrypted object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->get_data(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Encrypted.truncate(uint64_t  offset );
********************************************************/

static PyObject *pyEncrypted_truncate(pyEncrypted *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
static char *kwlist[] = {"offset", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "K|", kwlist, &offset))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Encrypted object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->truncate(((FileLikeObject)self->base), offset);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * Encrypted.close();
********************************************************/

static PyObject *pyEncrypted_close(pyEncrypted *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Encrypted object no longer valid");
// Precall preparations

// Make the call
((FileLikeObject)self->base)->close(((FileLikeObject)self->base));

// Postcall preparations
self->base = NULL;

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

};


static PyTypeObject Encrypted_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.Encrypted",               /* tp_name */
    sizeof(pyEncrypted),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)Encrypted_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)Encrypted_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "Encrypted: \n   Encryption is handled via two components, the encrypted stream and\n   the cipher. An encrypted stream is an FileLikeObject with the\n   following AFF4 attributes:\n\n   AFF4_CHUNK_SIZE = Data will be broken into these chunks and\n                     encrypted independantly.\n\n   AFF4_STORED     = Data will be stored on this backing stream.\n\n   AFF4_CIPHER     = This is an RDFValue which extends the AFF4Cipher\n                     class. More on that below.\n\n   When opened, the FileLikeObject is created by instantiating a\n   cipher from the AFF4_CIPHER attribute. Data is then divided into\n   chunks and each chunk is encrypted using the cipher, and then\n   written to the backing stream.\n\n   A cipher is a class which extends the AFF4Cipher baseclass.\n\n   Of course a valid cipher must also implement valid serialization\n   methods and also some way of key initialization. Chunks must be\n   whole multiples of blocksize.\n",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Encrypted_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyEncrypted_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef ZipFile_methods[] = {
     {"set_property",(PyCFunction)pyZipFile_set_property, METH_VARARGS|METH_KEYWORDS, "void * ZipFile.set_property(char * attribute ,RDFValue value );\n\n\n"},
     {"finish",(PyCFunction)pyZipFile_finish, METH_VARARGS|METH_KEYWORDS, "AFFObject ZipFile.finish();\n\n\n"},
     {"Con",(PyCFunction)pyZipFile_Con, METH_VARARGS|METH_KEYWORDS, "ZipFile ZipFile.Con(char * file_urn ,uint64_t  mode );\n\n\n A zip file is opened on a file like object "},
     {"open_member",(PyCFunction)pyZipFile_open_member, METH_VARARGS|METH_KEYWORDS, "FileLikeObject ZipFile.open_member(char * filename ,uint64_t  mode ,uint64_t  compression );\n\n\n This method opens an existing member or creates a new one. We\n return a file like object which may be used to read and write the\n member. If we open a member for writing the zip file will be locked\n (so another attempt to open a new member for writing will raise,\n until this member is promptly closed). The ZipFile must have been\n called with create_new_volume or append_volume before.\n\n DEFAULT(mode) = \"r\"\n DEFAULT(compression) = ZIP_DEFLATE\n"},
     {"close",(PyCFunction)pyZipFile_close, METH_VARARGS|METH_KEYWORDS, "void * ZipFile.close();\n\n\n This method flushes the central directory and finalises the\n file. The file may still be accessed for reading after this.\n"},
     {"writestr",(PyCFunction)pyZipFile_writestr, METH_VARARGS|METH_KEYWORDS, "uint64_t  ZipFile.writestr(char * filename ,char * data, int len,uint64_t  compression );\n\n\n A convenience function for storing a string as a new file (it\n basically calls open_member, writes the string then closes it).\n\n DEFAULT(compression) = ZIP_DEFLATE\n"},
     {"load_from",(PyCFunction)pyZipFile_load_from, METH_VARARGS|METH_KEYWORDS, "uint64_t  ZipFile.load_from(RDFURN fd_urn ,uint64_t  mode );\n\n\n Load an AFF4 volume from the URN specified. We parse all the RDF\n     serializations.\n\n     DEFAULT(mode) = \"r\"\n  "},
     {NULL}  /* Sentinel */
};
static void
ZipFile_dealloc(pyZipFile *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyZipFile_init(pyZipFile *self, PyObject *args, PyObject *kwds) {
Gen_wrapper *urn;
char mode=0; char *str_mode = "\x0";
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|", kwlist, &urn,&str_mode))
 goto error;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

self->base = CONSTRUCT(ZipFile, AFFObject, Con, NULL, urn->base, mode);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class ZipFile");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *ZipFile_getattr(pyZipFile *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("total_entries");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("original_member_size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("compressed_member_size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("offset_of_member_header");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("directory_offset");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("storage_urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("_didModify");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("type");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("epoch_time");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("compression_method");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("crc");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("compressed_size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("header_offset");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=ZipFile_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "urn")) {
    PyObject *py_result;
    Gen_wrapper *urn;


    {
       Object returned_object = (Object)(((AFFObject)self->base)->urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       urn = new_class_wrapper(returned_object);
       if(!urn) goto error;
    }
talloc_increase_ref_count(urn->base);

    py_result = (PyObject *)urn;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((AFFObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "total_entries")) {
    PyObject *py_result;
    uint64_t  total_entries;


    total_entries = (((ZipFile)self->base)->total_entries);

    py_result = PyLong_FromLong(total_entries);

    return py_result;
};
if(!strcmp(name, "original_member_size")) {
    PyObject *py_result;
    uint64_t  original_member_size;


    original_member_size = (((ZipFile)self->base)->original_member_size);

    py_result = PyLong_FromLong(original_member_size);

    return py_result;
};
if(!strcmp(name, "compressed_member_size")) {
    PyObject *py_result;
    uint64_t  compressed_member_size;


    compressed_member_size = (((ZipFile)self->base)->compressed_member_size);

    py_result = PyLong_FromLong(compressed_member_size);

    return py_result;
};
if(!strcmp(name, "offset_of_member_header")) {
    PyObject *py_result;
    uint64_t  offset_of_member_header;


    offset_of_member_header = (((ZipFile)self->base)->offset_of_member_header);

    py_result = PyLong_FromLong(offset_of_member_header);

    return py_result;
};
if(!strcmp(name, "directory_offset")) {
    PyObject *py_result;
    Gen_wrapper *directory_offset;


    {
       Object returned_object = (Object)(((ZipFile)self->base)->directory_offset);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       directory_offset = new_class_wrapper(returned_object);
       if(!directory_offset) goto error;
    }
talloc_increase_ref_count(directory_offset->base);

    py_result = (PyObject *)directory_offset;

    return py_result;
};
if(!strcmp(name, "storage_urn")) {
    PyObject *py_result;
    Gen_wrapper *storage_urn;


    {
       Object returned_object = (Object)(((ZipFile)self->base)->storage_urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       storage_urn = new_class_wrapper(returned_object);
       if(!storage_urn) goto error;
    }
talloc_increase_ref_count(storage_urn->base);

    py_result = (PyObject *)storage_urn;

    return py_result;
};
if(!strcmp(name, "_didModify")) {
    PyObject *py_result;
    Gen_wrapper *_didModify;


    {
       Object returned_object = (Object)(((ZipFile)self->base)->_didModify);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       _didModify = new_class_wrapper(returned_object);
       if(!_didModify) goto error;
    }
talloc_increase_ref_count(_didModify->base);

    py_result = (PyObject *)_didModify;

    return py_result;
};
if(!strcmp(name, "type")) {
    PyObject *py_result;
    Gen_wrapper *type;


    {
       Object returned_object = (Object)(((ZipFile)self->base)->type);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       type = new_class_wrapper(returned_object);
       if(!type) goto error;
    }
talloc_increase_ref_count(type->base);

    py_result = (PyObject *)type;

    return py_result;
};
if(!strcmp(name, "epoch_time")) {
    PyObject *py_result;
    Gen_wrapper *epoch_time;


    {
       Object returned_object = (Object)(((ZipFile)self->base)->epoch_time);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       epoch_time = new_class_wrapper(returned_object);
       if(!epoch_time) goto error;
    }
talloc_increase_ref_count(epoch_time->base);

    py_result = (PyObject *)epoch_time;

    return py_result;
};
if(!strcmp(name, "compression_method")) {
    PyObject *py_result;
    Gen_wrapper *compression_method;


    {
       Object returned_object = (Object)(((ZipFile)self->base)->compression_method);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       compression_method = new_class_wrapper(returned_object);
       if(!compression_method) goto error;
    }
talloc_increase_ref_count(compression_method->base);

    py_result = (PyObject *)compression_method;

    return py_result;
};
if(!strcmp(name, "crc")) {
    PyObject *py_result;
    Gen_wrapper *crc;


    {
       Object returned_object = (Object)(((ZipFile)self->base)->crc);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       crc = new_class_wrapper(returned_object);
       if(!crc) goto error;
    }
talloc_increase_ref_count(crc->base);

    py_result = (PyObject *)crc;

    return py_result;
};
if(!strcmp(name, "size")) {
    PyObject *py_result;
    Gen_wrapper *size;


    {
       Object returned_object = (Object)(((ZipFile)self->base)->size);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       size = new_class_wrapper(returned_object);
       if(!size) goto error;
    }
talloc_increase_ref_count(size->base);

    py_result = (PyObject *)size;

    return py_result;
};
if(!strcmp(name, "compressed_size")) {
    PyObject *py_result;
    Gen_wrapper *compressed_size;


    {
       Object returned_object = (Object)(((ZipFile)self->base)->compressed_size);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       compressed_size = new_class_wrapper(returned_object);
       if(!compressed_size) goto error;
    }
talloc_increase_ref_count(compressed_size->base);

    py_result = (PyObject *)compressed_size;

    return py_result;
};
if(!strcmp(name, "header_offset")) {
    PyObject *py_result;
    Gen_wrapper *header_offset;


    {
       Object returned_object = (Object)(((ZipFile)self->base)->header_offset);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       header_offset = new_class_wrapper(returned_object);
       if(!header_offset) goto error;
    }
talloc_increase_ref_count(header_offset->base);

    py_result = (PyObject *)header_offset;

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
void * ZipFile.set_property(char * attribute ,RDFValue value );
********************************************************/

static PyObject *pyZipFile_set_property(pyZipFile *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * attribute;
Gen_wrapper *value;
static char *kwlist[] = {"attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "sO|", kwlist, &attribute,&value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFile object no longer valid");
// Precall preparations

// Make the call
((AFFObject)self->base)->set_property(((AFFObject)self->base), attribute, value->base);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
AFFObject ZipFile.finish();
********************************************************/

static PyObject *pyZipFile_finish(pyZipFile *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFile object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((AFFObject)self->base)->finish(((AFFObject)self->base));

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations
self->base = NULL;

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    self->base = NULL;
return NULL;

};


/********************************************************
Autogenerated wrapper for function:
ZipFile ZipFile.Con(char * file_urn ,uint64_t  mode );
********************************************************/

static PyObject *pyZipFile_Con(pyZipFile *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
char * file_urn;
char mode=0; char *str_mode = "\x0";
static char *kwlist[] = {"file_urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "ss|", kwlist, &file_urn,&str_mode))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFile object no longer valid");
// Precall preparations

if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

// Make the call
{
       Object returned_object = (Object)((ZipFile)self->base)->Con(((ZipFile)self->base), file_urn, mode);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object ZipFile: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
FileLikeObject ZipFile.open_member(char * filename ,uint64_t  mode ,uint64_t  compression );
********************************************************/

static PyObject *pyZipFile_open_member(pyZipFile *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
char * filename;
char mode=0; char *str_mode = "r";
uint64_t  compression=ZIP_DEFLATE;
static char *kwlist[] = {"filename","mode","compression", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|sK", kwlist, &filename,&str_mode,&compression))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFile object no longer valid");
// Precall preparations

if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

// Make the call
{
       Object returned_object = (Object)((ZipFile)self->base)->open_member(((ZipFile)self->base), filename, mode, compression);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object FileLikeObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * ZipFile.close();
********************************************************/

static PyObject *pyZipFile_close(pyZipFile *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFile object no longer valid");
// Precall preparations

// Make the call
((ZipFile)self->base)->close(((ZipFile)self->base));

// Postcall preparations
self->base = NULL;

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  ZipFile.writestr(char * filename ,char * data, int len,uint64_t  compression );
********************************************************/

static PyObject *pyZipFile_writestr(pyZipFile *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char * filename;
char *data=""; Py_ssize_t len=strlen("");
uint64_t  compression=ZIP_DEFLATE;
static char *kwlist[] = {"filename","data","compression", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "ss#|K", kwlist, &filename,&data, &len,&compression))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFile object no longer valid");
// Precall preparations

// Make the call
func_return = ((ZipFile)self->base)->writestr(((ZipFile)self->base), filename, (char *)data, (int)len, compression);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  ZipFile.load_from(RDFURN fd_urn ,uint64_t  mode );
********************************************************/

static PyObject *pyZipFile_load_from(pyZipFile *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
Gen_wrapper *fd_urn;
char mode=0; char *str_mode = "r";
static char *kwlist[] = {"fd_urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|s", kwlist, &fd_urn,&str_mode))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFile object no longer valid");
// Precall preparations

if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

// Make the call
func_return = ((ZipFile)self->base)->load_from(((ZipFile)self->base), fd_urn->base, mode);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


static PyTypeObject ZipFile_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.ZipFile",               /* tp_name */
    sizeof(pyZipFile),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)ZipFile_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)ZipFile_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "ZipFile:  This represents a Zip file ",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    ZipFile_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyZipFile_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef XSDDatetime_methods[] = {
     {"parse",(PyCFunction)pyXSDDatetime_parse, METH_VARARGS|METH_KEYWORDS, "uint64_t  XSDDatetime.parse(char * serialised_form );\n\n\n"},
     {"encode",(PyCFunction)pyXSDDatetime_encode, METH_VARARGS|METH_KEYWORDS, "TDB_DATA * XSDDatetime.encode();\n\n\n"},
     {"decode",(PyCFunction)pyXSDDatetime_decode, METH_VARARGS|METH_KEYWORDS, "uint64_t  XSDDatetime.decode(char * data, int length);\n\n\n"},
     {"serialise",(PyCFunction)pyXSDDatetime_serialise, METH_VARARGS|METH_KEYWORDS, "char * XSDDatetime.serialise();\n\n\n"},
     {"set",(PyCFunction)pyXSDDatetime_set, METH_VARARGS|METH_KEYWORDS, "RDFValue XSDDatetime.set(struct timeval time );\n\n\n"},
     {NULL}  /* Sentinel */
};
static void
XSDDatetime_dealloc(pyXSDDatetime *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyXSDDatetime_init(pyXSDDatetime *self, PyObject *args, PyObject *kwds) {

self->base = CONSTRUCT(XSDDatetime, RDFValue, Con, NULL);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class XSDDatetime");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *XSDDatetime_getattr(pyXSDDatetime *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("dataType");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("id");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("value");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("serialised");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=XSDDatetime_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "dataType")) {
    PyObject *py_result;
    char * dataType;


    dataType = (((RDFValue)self->base)->dataType);

    py_result = PyString_FromStringAndSize((char *)dataType, strlen(dataType));

    return py_result;
};
if(!strcmp(name, "id")) {
    PyObject *py_result;
    uint64_t  id;


    id = (((RDFValue)self->base)->id);

    py_result = PyLong_FromLong(id);

    return py_result;
};
if(!strcmp(name, "value")) {
    PyObject *py_result;
    float value_flt; struct timeval value;


    value = (((XSDDatetime)self->base)->value);

    value_flt = (double)(value.tv_sec) + value.tv_usec;
py_result = PyFloat_FromDouble(value_flt);

    return py_result;
};
if(!strcmp(name, "serialised")) {
    PyObject *py_result;
    char * serialised;


    serialised = (((XSDDatetime)self->base)->serialised);

    py_result = PyString_FromStringAndSize((char *)serialised, strlen(serialised));

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDDatetime.parse(char * serialised_form );
********************************************************/

static PyObject *pyXSDDatetime_parse(pyXSDDatetime *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char * serialised_form;
static char *kwlist[] = {"serialised_form", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &serialised_form))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDDatetime object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->parse(((RDFValue)self->base), serialised_form);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA * XSDDatetime.encode();
********************************************************/

static PyObject *pyXSDDatetime_encode(pyXSDDatetime *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
TDB_DATA * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDDatetime object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->encode(((RDFValue)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return->dptr, func_return->dsize);
talloc_free(func_return);returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDDatetime.decode(char * data, int length);
********************************************************/

static PyObject *pyXSDDatetime_decode(pyXSDDatetime *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *data=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"data", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &data, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDDatetime object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->decode(((RDFValue)self->base), (char *)data, (int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
char * XSDDatetime.serialise();
********************************************************/

static PyObject *pyXSDDatetime_serialise(pyXSDDatetime *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDDatetime object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->serialise(((RDFValue)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
RDFValue XSDDatetime.set(struct timeval time );
********************************************************/

static PyObject *pyXSDDatetime_set(pyXSDDatetime *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
float time_flt; struct timeval time;
static char *kwlist[] = {"time", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "f|", kwlist, &time_flt))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDDatetime object no longer valid");
// Precall preparations
time.tv_sec = (int)time_flt; time.tv_usec = (time_flt - time.tv_sec) * 1e6;

// Make the call
{
       Object returned_object = (Object)((XSDDatetime)self->base)->set(((XSDDatetime)self->base), time);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFValue: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }
talloc_increase_ref_count(func_return->base);

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


static PyTypeObject XSDDatetime_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.XSDDatetime",               /* tp_name */
    sizeof(pyXSDDatetime),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)XSDDatetime_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)XSDDatetime_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "XSDDatetime:  Dates serialised according the XML standard ",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    XSDDatetime_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyXSDDatetime_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef URLParse_methods[] = {
     {"parse",(PyCFunction)pyURLParse_parse, METH_VARARGS|METH_KEYWORDS, "uint64_t  URLParse.parse(char * url );\n\n\n Parses the url given and sets internal attributes.\n"},
     {"string",(PyCFunction)pyURLParse_string, METH_VARARGS|METH_KEYWORDS, "char * URLParse.string(void * None );\n\n\n Returns the internal attributes joined together into a valid URL.\n"},
     {NULL}  /* Sentinel */
};
static void
URLParse_dealloc(pyURLParse *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyURLParse_init(pyURLParse *self, PyObject *args, PyObject *kwds) {
char * url;
static char *kwlist[] = {"url", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &url))
 goto error;


self->base = CONSTRUCT(URLParse, URLParse, Con, NULL, url);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class URLParse");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *URLParse_getattr(pyURLParse *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("scheme");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("netloc");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("query");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("fragment");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=URLParse_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "scheme")) {
    PyObject *py_result;
    char * scheme;


    scheme = (((URLParse)self->base)->scheme);

    py_result = PyString_FromStringAndSize((char *)scheme, strlen(scheme));

    return py_result;
};
if(!strcmp(name, "netloc")) {
    PyObject *py_result;
    char * netloc;


    netloc = (((URLParse)self->base)->netloc);

    py_result = PyString_FromStringAndSize((char *)netloc, strlen(netloc));

    return py_result;
};
if(!strcmp(name, "query")) {
    PyObject *py_result;
    char * query;


    query = (((URLParse)self->base)->query);

    py_result = PyString_FromStringAndSize((char *)query, strlen(query));

    return py_result;
};
if(!strcmp(name, "fragment")) {
    PyObject *py_result;
    char * fragment;


    fragment = (((URLParse)self->base)->fragment);

    py_result = PyString_FromStringAndSize((char *)fragment, strlen(fragment));

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
uint64_t  URLParse.parse(char * url );
********************************************************/

static PyObject *pyURLParse_parse(pyURLParse *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char * url;
static char *kwlist[] = {"url", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &url))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "URLParse object no longer valid");
// Precall preparations

// Make the call
func_return = ((URLParse)self->base)->parse(((URLParse)self->base), url);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
char * URLParse.string(void * None );
********************************************************/

static PyObject *pyURLParse_string(pyURLParse *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "URLParse object no longer valid");
// Precall preparations

// Make the call
func_return = ((URLParse)self->base)->string(((URLParse)self->base), NULL);

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;

};


static PyTypeObject URLParse_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.URLParse",               /* tp_name */
    sizeof(pyURLParse),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)URLParse_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)URLParse_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "URLParse:  A class used to parse URNs ",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    URLParse_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyURLParse_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef RDFValue_methods[] = {
     {"parse",(PyCFunction)pyRDFValue_parse, METH_VARARGS|METH_KEYWORDS, "uint64_t  RDFValue.parse(char * serialised_form );\n\n\n This method is called to parse a serialised form into this\n\t instance. Return 1 if parsing is successful, 0 if error\n\t occured. \n      "},
     {"encode",(PyCFunction)pyRDFValue_encode, METH_VARARGS|METH_KEYWORDS, "TDB_DATA * RDFValue.encode();\n\n\n This method is called to serialise this object into the\n\t TDB_DATA struct for storage in the TDB data store. The new\n\t memory will be allocated with this object\'s context and must\n\t be freed by the caller.\n      "},
     {"decode",(PyCFunction)pyRDFValue_decode, METH_VARARGS|METH_KEYWORDS, "uint64_t  RDFValue.decode(char * data, int length);\n\n\n This method is used to decode this object from the\n data_store. The fd is seeked to the start of this record.\n"},
     {"serialise",(PyCFunction)pyRDFValue_serialise, METH_VARARGS|METH_KEYWORDS, "char * RDFValue.serialise();\n\n\n This method will serialise the value into a null terminated\n\t  string for export into RDF. The returned string will be\n\t  allocated internally and should not be freed by the caller. \n      "},
     {NULL}  /* Sentinel */
};
static void
RDFValue_dealloc(pyRDFValue *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyRDFValue_init(pyRDFValue *self, PyObject *args, PyObject *kwds) {

self->base = CONSTRUCT(RDFValue, RDFValue, Con, NULL);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class RDFValue");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *RDFValue_getattr(pyRDFValue *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("dataType");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("id");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=RDFValue_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "dataType")) {
    PyObject *py_result;
    char * dataType;


    dataType = (((RDFValue)self->base)->dataType);

    py_result = PyString_FromStringAndSize((char *)dataType, strlen(dataType));

    return py_result;
};
if(!strcmp(name, "id")) {
    PyObject *py_result;
    uint64_t  id;


    id = (((RDFValue)self->base)->id);

    py_result = PyLong_FromLong(id);

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
uint64_t  RDFValue.parse(char * serialised_form );
********************************************************/

static PyObject *pyRDFValue_parse(pyRDFValue *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char * serialised_form;
static char *kwlist[] = {"serialised_form", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &serialised_form))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFValue object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->parse(((RDFValue)self->base), serialised_form);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA * RDFValue.encode();
********************************************************/

static PyObject *pyRDFValue_encode(pyRDFValue *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
TDB_DATA * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFValue object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->encode(((RDFValue)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return->dptr, func_return->dsize);
talloc_free(func_return);returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  RDFValue.decode(char * data, int length);
********************************************************/

static PyObject *pyRDFValue_decode(pyRDFValue *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *data=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"data", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &data, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFValue object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->decode(((RDFValue)self->base), (char *)data, (int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
char * RDFValue.serialise();
********************************************************/

static PyObject *pyRDFValue_serialise(pyRDFValue *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFValue object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->serialise(((RDFValue)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
returned_result = py_result;
return returned_result;

};


static PyTypeObject RDFValue_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.RDFValue",               /* tp_name */
    sizeof(pyRDFValue),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)RDFValue_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)RDFValue_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "RDFValue:  The RDF resolver stores objects inherited from this one.\n\n       You can define other objects and register them using the\n       register_rdf_value_class() function. You will need to extend\n       this base class and initialise it with a unique dataType URN.\n\n       RDFValue is the base class for all other values.\n",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    RDFValue_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyRDFValue_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef HTTPObject_methods[] = {
     {"set_property",(PyCFunction)pyHTTPObject_set_property, METH_VARARGS|METH_KEYWORDS, "void * HTTPObject.set_property(char * attribute ,RDFValue value );\n\n\n"},
     {"finish",(PyCFunction)pyHTTPObject_finish, METH_VARARGS|METH_KEYWORDS, "AFFObject HTTPObject.finish();\n\n\n"},
     {"seek",(PyCFunction)pyHTTPObject_seek, METH_VARARGS|METH_KEYWORDS, "uint64_t  HTTPObject.seek(uint64_t  offset ,uint64_t  whence );\n\n\n"},
     {"read",(PyCFunction)pyHTTPObject_read, METH_VARARGS|METH_KEYWORDS, "uint64_t  HTTPObject.read(OUT char * buffer, unsigned long int length);\n\n\n"},
     {"write",(PyCFunction)pyHTTPObject_write, METH_VARARGS|METH_KEYWORDS, "uint64_t  HTTPObject.write(char * buffer, unsigned long int length);\n\n\n"},
     {"tell",(PyCFunction)pyHTTPObject_tell, METH_VARARGS|METH_KEYWORDS, "uint64_t  HTTPObject.tell();\n\n\n"},
     {"get_data",(PyCFunction)pyHTTPObject_get_data, METH_VARARGS|METH_KEYWORDS, "char * HTTPObject.get_data();\n\n\n"},
     {"truncate",(PyCFunction)pyHTTPObject_truncate, METH_VARARGS|METH_KEYWORDS, "uint64_t  HTTPObject.truncate(uint64_t  offset );\n\n\n"},
     {"close",(PyCFunction)pyHTTPObject_close, METH_VARARGS|METH_KEYWORDS, "void * HTTPObject.close();\n\n\n"},
     {"Con",(PyCFunction)pyHTTPObject_Con, METH_VARARGS|METH_KEYWORDS, "HTTPObject HTTPObject.Con(char * url );\n\n\n"},
     {NULL}  /* Sentinel */
};
static void
HTTPObject_dealloc(pyHTTPObject *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyHTTPObject_init(pyHTTPObject *self, PyObject *args, PyObject *kwds) {
Gen_wrapper *urn;
char mode=0; char *str_mode = "\x0";
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|", kwlist, &urn,&str_mode))
 goto error;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

self->base = CONSTRUCT(HTTPObject, AFFObject, Con, NULL, urn->base, mode);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class HTTPObject");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *HTTPObject_getattr(pyHTTPObject *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("readptr");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("data");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("send_buffer_offset");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=HTTPObject_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "urn")) {
    PyObject *py_result;
    Gen_wrapper *urn;


    {
       Object returned_object = (Object)(((AFFObject)self->base)->urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       urn = new_class_wrapper(returned_object);
       if(!urn) goto error;
    }
talloc_increase_ref_count(urn->base);

    py_result = (PyObject *)urn;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((AFFObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "readptr")) {
    PyObject *py_result;
    uint64_t  readptr;


    readptr = (((FileLikeObject)self->base)->readptr);

    py_result = PyLong_FromLong(readptr);

    return py_result;
};
if(!strcmp(name, "size")) {
    PyObject *py_result;
    Gen_wrapper *size;


    {
       Object returned_object = (Object)(((FileLikeObject)self->base)->size);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       size = new_class_wrapper(returned_object);
       if(!size) goto error;
    }
talloc_increase_ref_count(size->base);

    py_result = (PyObject *)size;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((FileLikeObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "data")) {
    PyObject *py_result;
    char * data;


    data = (((FileLikeObject)self->base)->data);

    py_result = PyString_FromStringAndSize((char *)data, strlen(data));

    return py_result;
};
if(!strcmp(name, "send_buffer_offset")) {
    PyObject *py_result;
    uint64_t  send_buffer_offset;


    send_buffer_offset = (((HTTPObject)self->base)->send_buffer_offset);

    py_result = PyLong_FromLong(send_buffer_offset);

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
void * HTTPObject.set_property(char * attribute ,RDFValue value );
********************************************************/

static PyObject *pyHTTPObject_set_property(pyHTTPObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * attribute;
Gen_wrapper *value;
static char *kwlist[] = {"attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "sO|", kwlist, &attribute,&value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "HTTPObject object no longer valid");
// Precall preparations

// Make the call
((AFFObject)self->base)->set_property(((AFFObject)self->base), attribute, value->base);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
AFFObject HTTPObject.finish();
********************************************************/

static PyObject *pyHTTPObject_finish(pyHTTPObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "HTTPObject object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((AFFObject)self->base)->finish(((AFFObject)self->base));

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations
self->base = NULL;

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    self->base = NULL;
return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  HTTPObject.seek(uint64_t  offset ,uint64_t  whence );
********************************************************/

static PyObject *pyHTTPObject_seek(pyHTTPObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
uint64_t  whence;
static char *kwlist[] = {"offset","whence", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "KK|", kwlist, &offset,&whence))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "HTTPObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->seek(((FileLikeObject)self->base), offset, whence);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  HTTPObject.read(OUT char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyHTTPObject_read(pyHTTPObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
PyObject *tmp_buffer;
static char *kwlist[] = {"length", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "l|", kwlist, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "HTTPObject object no longer valid");
// Precall preparations
tmp_buffer = PyString_FromStringAndSize(NULL, length);
PyString_AsStringAndSize(tmp_buffer, &buffer, (Py_ssize_t *)&length);

// Make the call
func_return = ((FileLikeObject)self->base)->read(((FileLikeObject)self->base), (OUT char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
returned_result = PyList_New(0);
py_result = PyLong_FromLong(func_return);
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
 _PyString_Resize(&tmp_buffer, func_return); 
py_result = tmp_buffer;
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  HTTPObject.write(char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyHTTPObject_write(pyHTTPObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"buffer", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &buffer, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "HTTPObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->write(((FileLikeObject)self->base), (char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  HTTPObject.tell();
********************************************************/

static PyObject *pyHTTPObject_tell(pyHTTPObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "HTTPObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->tell(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
char * HTTPObject.get_data();
********************************************************/

static PyObject *pyHTTPObject_get_data(pyHTTPObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "HTTPObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->get_data(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  HTTPObject.truncate(uint64_t  offset );
********************************************************/

static PyObject *pyHTTPObject_truncate(pyHTTPObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
static char *kwlist[] = {"offset", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "K|", kwlist, &offset))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "HTTPObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->truncate(((FileLikeObject)self->base), offset);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * HTTPObject.close();
********************************************************/

static PyObject *pyHTTPObject_close(pyHTTPObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "HTTPObject object no longer valid");
// Precall preparations

// Make the call
((FileLikeObject)self->base)->close(((FileLikeObject)self->base));

// Postcall preparations
self->base = NULL;

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
HTTPObject HTTPObject.Con(char * url );
********************************************************/

static PyObject *pyHTTPObject_Con(pyHTTPObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
char * url;
static char *kwlist[] = {"url", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &url))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "HTTPObject object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((HTTPObject)self->base)->Con(((HTTPObject)self->base), url);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object HTTPObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


static PyTypeObject HTTPObject_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.HTTPObject",               /* tp_name */
    sizeof(pyHTTPObject),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)HTTPObject_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)HTTPObject_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "HTTPObject:  This is implemented in using libcurl\n",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    HTTPObject_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyHTTPObject_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef ZipFileStream_methods[] = {
     {"set_property",(PyCFunction)pyZipFileStream_set_property, METH_VARARGS|METH_KEYWORDS, "void * ZipFileStream.set_property(char * attribute ,RDFValue value );\n\n\n"},
     {"finish",(PyCFunction)pyZipFileStream_finish, METH_VARARGS|METH_KEYWORDS, "AFFObject ZipFileStream.finish();\n\n\n"},
     {"seek",(PyCFunction)pyZipFileStream_seek, METH_VARARGS|METH_KEYWORDS, "uint64_t  ZipFileStream.seek(uint64_t  offset ,uint64_t  whence );\n\n\n"},
     {"read",(PyCFunction)pyZipFileStream_read, METH_VARARGS|METH_KEYWORDS, "uint64_t  ZipFileStream.read(OUT char * buffer, unsigned long int length);\n\n\n"},
     {"write",(PyCFunction)pyZipFileStream_write, METH_VARARGS|METH_KEYWORDS, "uint64_t  ZipFileStream.write(char * buffer, unsigned long int length);\n\n\n"},
     {"tell",(PyCFunction)pyZipFileStream_tell, METH_VARARGS|METH_KEYWORDS, "uint64_t  ZipFileStream.tell();\n\n\n"},
     {"get_data",(PyCFunction)pyZipFileStream_get_data, METH_VARARGS|METH_KEYWORDS, "char * ZipFileStream.get_data();\n\n\n"},
     {"truncate",(PyCFunction)pyZipFileStream_truncate, METH_VARARGS|METH_KEYWORDS, "uint64_t  ZipFileStream.truncate(uint64_t  offset );\n\n\n"},
     {"close",(PyCFunction)pyZipFileStream_close, METH_VARARGS|METH_KEYWORDS, "void * ZipFileStream.close();\n\n\n"},
     {"Con",(PyCFunction)pyZipFileStream_Con, METH_VARARGS|METH_KEYWORDS, "ZipFileStream ZipFileStream.Con(RDFURN urn ,RDFURN file_urn ,RDFURN container_urn ,uint64_t  mode ,FileLikeObject file_fd );\n\n\n This is the constructor for the file like object. \n\tfile_urn is the storage file for the volume in\n\tcontainer_urn. If the stream is opened for writing the file_fd\n\tmay be passed in. It remains locked until we are closed.\n     "},
     {NULL}  /* Sentinel */
};
static void
ZipFileStream_dealloc(pyZipFileStream *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyZipFileStream_init(pyZipFileStream *self, PyObject *args, PyObject *kwds) {
Gen_wrapper *urn;
char mode=0; char *str_mode = "\x0";
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|", kwlist, &urn,&str_mode))
 goto error;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

self->base = CONSTRUCT(ZipFileStream, AFFObject, Con, NULL, urn->base, mode);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class ZipFileStream");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *ZipFileStream_getattr(pyZipFileStream *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("readptr");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("data");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("file_offset");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("file_urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("container_urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("file_fd");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("compress_size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("compression");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("cbuff");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("buff");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=ZipFileStream_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "urn")) {
    PyObject *py_result;
    Gen_wrapper *urn;


    {
       Object returned_object = (Object)(((AFFObject)self->base)->urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       urn = new_class_wrapper(returned_object);
       if(!urn) goto error;
    }
talloc_increase_ref_count(urn->base);

    py_result = (PyObject *)urn;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((AFFObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "readptr")) {
    PyObject *py_result;
    uint64_t  readptr;


    readptr = (((FileLikeObject)self->base)->readptr);

    py_result = PyLong_FromLong(readptr);

    return py_result;
};
if(!strcmp(name, "size")) {
    PyObject *py_result;
    Gen_wrapper *size;


    {
       Object returned_object = (Object)(((FileLikeObject)self->base)->size);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       size = new_class_wrapper(returned_object);
       if(!size) goto error;
    }
talloc_increase_ref_count(size->base);

    py_result = (PyObject *)size;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((FileLikeObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "data")) {
    PyObject *py_result;
    char * data;


    data = (((FileLikeObject)self->base)->data);

    py_result = PyString_FromStringAndSize((char *)data, strlen(data));

    return py_result;
};
if(!strcmp(name, "file_offset")) {
    PyObject *py_result;
    Gen_wrapper *file_offset;


    {
       Object returned_object = (Object)(((ZipFileStream)self->base)->file_offset);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       file_offset = new_class_wrapper(returned_object);
       if(!file_offset) goto error;
    }
talloc_increase_ref_count(file_offset->base);

    py_result = (PyObject *)file_offset;

    return py_result;
};
if(!strcmp(name, "file_urn")) {
    PyObject *py_result;
    Gen_wrapper *file_urn;


    {
       Object returned_object = (Object)(((ZipFileStream)self->base)->file_urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       file_urn = new_class_wrapper(returned_object);
       if(!file_urn) goto error;
    }
talloc_increase_ref_count(file_urn->base);

    py_result = (PyObject *)file_urn;

    return py_result;
};
if(!strcmp(name, "container_urn")) {
    PyObject *py_result;
    Gen_wrapper *container_urn;


    {
       Object returned_object = (Object)(((ZipFileStream)self->base)->container_urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       container_urn = new_class_wrapper(returned_object);
       if(!container_urn) goto error;
    }
talloc_increase_ref_count(container_urn->base);

    py_result = (PyObject *)container_urn;

    return py_result;
};
if(!strcmp(name, "file_fd")) {
    PyObject *py_result;
    Gen_wrapper *file_fd;


    {
       Object returned_object = (Object)(((ZipFileStream)self->base)->file_fd);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object FileLikeObject: %s", __error_str);
         ClearError();
         goto error;
       };

       file_fd = new_class_wrapper(returned_object);
       if(!file_fd) goto error;
    }
talloc_increase_ref_count(file_fd->base);

    py_result = (PyObject *)file_fd;

    return py_result;
};
if(!strcmp(name, "compress_size")) {
    PyObject *py_result;
    Gen_wrapper *compress_size;


    {
       Object returned_object = (Object)(((ZipFileStream)self->base)->compress_size);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       compress_size = new_class_wrapper(returned_object);
       if(!compress_size) goto error;
    }
talloc_increase_ref_count(compress_size->base);

    py_result = (PyObject *)compress_size;

    return py_result;
};
if(!strcmp(name, "compression")) {
    PyObject *py_result;
    Gen_wrapper *compression;


    {
       Object returned_object = (Object)(((ZipFileStream)self->base)->compression);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       compression = new_class_wrapper(returned_object);
       if(!compression) goto error;
    }
talloc_increase_ref_count(compression->base);

    py_result = (PyObject *)compression;

    return py_result;
};
if(!strcmp(name, "cbuff")) {
    PyObject *py_result;
    char * cbuff;


    cbuff = (((ZipFileStream)self->base)->cbuff);

    py_result = PyString_FromStringAndSize((char *)cbuff, strlen(cbuff));

    return py_result;
};
if(!strcmp(name, "buff")) {
    PyObject *py_result;
    char * buff;


    buff = (((ZipFileStream)self->base)->buff);

    py_result = PyString_FromStringAndSize((char *)buff, strlen(buff));

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
void * ZipFileStream.set_property(char * attribute ,RDFValue value );
********************************************************/

static PyObject *pyZipFileStream_set_property(pyZipFileStream *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * attribute;
Gen_wrapper *value;
static char *kwlist[] = {"attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "sO|", kwlist, &attribute,&value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFileStream object no longer valid");
// Precall preparations

// Make the call
((AFFObject)self->base)->set_property(((AFFObject)self->base), attribute, value->base);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
AFFObject ZipFileStream.finish();
********************************************************/

static PyObject *pyZipFileStream_finish(pyZipFileStream *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFileStream object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((AFFObject)self->base)->finish(((AFFObject)self->base));

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations
self->base = NULL;

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    self->base = NULL;
return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  ZipFileStream.seek(uint64_t  offset ,uint64_t  whence );
********************************************************/

static PyObject *pyZipFileStream_seek(pyZipFileStream *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
uint64_t  whence;
static char *kwlist[] = {"offset","whence", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "KK|", kwlist, &offset,&whence))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFileStream object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->seek(((FileLikeObject)self->base), offset, whence);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  ZipFileStream.read(OUT char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyZipFileStream_read(pyZipFileStream *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
PyObject *tmp_buffer;
static char *kwlist[] = {"length", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "l|", kwlist, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFileStream object no longer valid");
// Precall preparations
tmp_buffer = PyString_FromStringAndSize(NULL, length);
PyString_AsStringAndSize(tmp_buffer, &buffer, (Py_ssize_t *)&length);

// Make the call
func_return = ((FileLikeObject)self->base)->read(((FileLikeObject)self->base), (OUT char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
returned_result = PyList_New(0);
py_result = PyLong_FromLong(func_return);
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
 _PyString_Resize(&tmp_buffer, func_return); 
py_result = tmp_buffer;
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  ZipFileStream.write(char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyZipFileStream_write(pyZipFileStream *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"buffer", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &buffer, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFileStream object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->write(((FileLikeObject)self->base), (char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  ZipFileStream.tell();
********************************************************/

static PyObject *pyZipFileStream_tell(pyZipFileStream *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFileStream object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->tell(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
char * ZipFileStream.get_data();
********************************************************/

static PyObject *pyZipFileStream_get_data(pyZipFileStream *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFileStream object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->get_data(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  ZipFileStream.truncate(uint64_t  offset );
********************************************************/

static PyObject *pyZipFileStream_truncate(pyZipFileStream *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
static char *kwlist[] = {"offset", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "K|", kwlist, &offset))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFileStream object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->truncate(((FileLikeObject)self->base), offset);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * ZipFileStream.close();
********************************************************/

static PyObject *pyZipFileStream_close(pyZipFileStream *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFileStream object no longer valid");
// Precall preparations

// Make the call
((FileLikeObject)self->base)->close(((FileLikeObject)self->base));

// Postcall preparations
self->base = NULL;

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
ZipFileStream ZipFileStream.Con(RDFURN urn ,RDFURN file_urn ,RDFURN container_urn ,uint64_t  mode ,FileLikeObject file_fd );
********************************************************/

static PyObject *pyZipFileStream_Con(pyZipFileStream *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
Gen_wrapper *urn;
Gen_wrapper *file_urn;
Gen_wrapper *container_urn;
char mode=0; char *str_mode = "\x0";
Gen_wrapper *file_fd;
static char *kwlist[] = {"urn","file_urn","container_urn","mode","file_fd", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "OOOsO|", kwlist, &urn,&file_urn,&container_urn,&str_mode,&file_fd))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "ZipFileStream object no longer valid");
// Precall preparations

if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

// Make the call
{
       Object returned_object = (Object)((ZipFileStream)self->base)->Con(((ZipFileStream)self->base), urn->base, file_urn->base, container_urn->base, mode, file_fd->base);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object ZipFileStream: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


static PyTypeObject ZipFileStream_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.ZipFileStream",               /* tp_name */
    sizeof(pyZipFileStream),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)ZipFileStream_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)ZipFileStream_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "ZipFileStream:  This is a FileLikeObject which is used to provide access to zip\n archive members. Currently only accessible through\n ZipFile.open_member()\n",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    ZipFileStream_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyZipFileStream_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef Resolver_methods[] = {
     {"open",(PyCFunction)pyResolver_open, METH_VARARGS|METH_KEYWORDS, "AFFObject Resolver.open(RDFURN uri ,uint64_t  mode );\n\n\n This method tries to resolve the provided uri and returns an\n instance of whatever the URI refers to (As an AFFObject which is the\n common base class. You should check to see that what you get back is\n actually what you need. For example:\n\n FileLikeObject fd = (FileLikeObject)CALL(resolver, resolve, uri);\n if(!fd || !ISSUBCLASS(fd, FileLikeObject)) goto error;\n \n Once the resolver provides the object it is attached to the context\n ctx and removed from the cache. This ensures that the object can not\n expire from the cache while callers are holding it. You must return\n the object to the cache as soon as possible by calling\n cache_return. The object will be locked until you return it with\n cache_return. \n\n DEFAULT(mode) = \"r\"\n"},
     {"cache_return",(PyCFunction)pyResolver_cache_return, METH_VARARGS|METH_KEYWORDS, "void * Resolver.cache_return(AFFObject obj );\n\n\n All objected obtained from Resolver.open() need to be\n returned to the cache promptly using this method. NOTE - it\n is an error to be using an object after being returned to\n the cache - it might not be valid and may be gced.\n"},
     {"create",(PyCFunction)pyResolver_create, METH_VARARGS|METH_KEYWORDS, "AFFObject Resolver.create(char * name ,uint64_t  mode );\n\n\n This create a new object of the specified type.\n\n          name specifies the type of object as registered in the type\n          handler dispatcher. (e.g. AFF4_ZIP_VOLUME)\n\n          DEFAULT(mode) = \"w\"\n        "},
     {"resolve_value",(PyCFunction)pyResolver_resolve_value, METH_VARARGS|METH_KEYWORDS, "uint64_t  Resolver.resolve_value(RDFURN uri ,char * attribute ,RDFValue value );\n\n\n This function resolves the value in uri and attribute and sets it\n     into the RDFValue object. So you must first create such an object\n     (e.g. XSDDatetime) and then pass the object here to be updated\n     from the data store. Note that only a single value is returned -\n     if you want to iterate over all the values for this attribute you\n     need to call get_iter and iter_next.\n  "},
     {"get_iter",(PyCFunction)pyResolver_get_iter, METH_VARARGS|METH_KEYWORDS, "RESOLVER_ITER Resolver.get_iter(void * None ,RDFURN uri ,char * attribute );\n\n\n This is a version of the above which uses an iterator to iterate\n     over the list.\n\n     The iterator is obtained using get_iter first. This function\n     returns 1 if an iterator can be found (i.e. at least one result\n     exists) or 0 if no results exist.\n\n     Each call to iter_next will write a new value into the buffer set\n     up by result with maximal length length. Only results matching\n     the type specified are returned. We return length written for\n     each successful iteration, and zero when we have no more items.\n  "},
     {"iter_next",(PyCFunction)pyResolver_iter_next, METH_VARARGS|METH_KEYWORDS, "uint64_t  Resolver.iter_next(RESOLVER_ITER iter ,RDFValue result );\n\n\n This method reads the next result from the iterator. result\n\t  must be an allocated and valid RDFValue object "},
     {"iter_next_alloc",(PyCFunction)pyResolver_iter_next_alloc, METH_VARARGS|METH_KEYWORDS, "RDFValue Resolver.iter_next_alloc(RESOLVER_ITER iter );\n\n\n This method is similar to iter_next except the result is\n\t  allocated. Callers need to talloc_free the result. This\n\t  advantage of this method is that we dont need to know in\n\t  advance what type the value is.\n       "},
     {"del",(PyCFunction)pyResolver_del, METH_VARARGS|METH_KEYWORDS, "void * Resolver.del(RDFURN uri ,char * attribute );\n\n\n Deletes all values for this attribute from the resolver\n"},
     {"set_value",(PyCFunction)pyResolver_set_value, METH_VARARGS|METH_KEYWORDS, "void * Resolver.set_value(RDFURN uri ,char * attribute ,RDFValue value );\n\n\n Sets a new value for an attribute. Note that this function\n clears any previously set values, if you want to create a list\n of values you need to call add_value.\n"},
     {"add_value",(PyCFunction)pyResolver_add_value, METH_VARARGS|METH_KEYWORDS, "void * Resolver.add_value(RDFURN uri ,char * attribute ,RDFValue value );\n\n\n Adds a new value to the value list for this attribute.\n"},
     {NULL}  /* Sentinel */
};
static void
Resolver_dealloc(pyResolver *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyResolver_init(pyResolver *self, PyObject *args, PyObject *kwds) {

self->base = CONSTRUCT(Resolver, Resolver, Con, NULL);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class Resolver");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *Resolver_getattr(pyResolver *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("data_store_fd");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("hashsize");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("message");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("type");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=Resolver_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "data_store_fd")) {
    PyObject *py_result;
    uint64_t  data_store_fd;


    data_store_fd = (((Resolver)self->base)->data_store_fd);

    py_result = PyLong_FromLong(data_store_fd);

    return py_result;
};
if(!strcmp(name, "hashsize")) {
    PyObject *py_result;
    uint64_t  hashsize;


    hashsize = (((Resolver)self->base)->hashsize);

    py_result = PyLong_FromLong(hashsize);

    return py_result;
};
if(!strcmp(name, "message")) {
    PyObject *py_result;
    char * message;


    message = (((Resolver)self->base)->message);

    py_result = PyString_FromStringAndSize((char *)message, strlen(message));

    return py_result;
};
if(!strcmp(name, "type")) {
    PyObject *py_result;
    Gen_wrapper *type;


    {
       Object returned_object = (Object)(((Resolver)self->base)->type);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDString: %s", __error_str);
         ClearError();
         goto error;
       };

       type = new_class_wrapper(returned_object);
       if(!type) goto error;
    }
talloc_increase_ref_count(type->base);

    py_result = (PyObject *)type;

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
AFFObject Resolver.open(RDFURN uri ,uint64_t  mode );
********************************************************/

static PyObject *pyResolver_open(pyResolver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
Gen_wrapper *uri;
char mode=0; char *str_mode = "r";
static char *kwlist[] = {"uri","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|s", kwlist, &uri,&str_mode))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Resolver object no longer valid");
// Precall preparations

if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

// Make the call
{
       Object returned_object = (Object)((Resolver)self->base)->open(((Resolver)self->base), uri->base, mode);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * Resolver.cache_return(AFFObject obj );
********************************************************/

static PyObject *pyResolver_cache_return(pyResolver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *obj;
static char *kwlist[] = {"obj", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|", kwlist, &obj))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Resolver object no longer valid");
// Precall preparations

// Make the call
((Resolver)self->base)->cache_return(((Resolver)self->base), obj->base);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
AFFObject Resolver.create(char * name ,uint64_t  mode );
********************************************************/

static PyObject *pyResolver_create(pyResolver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
char * name;
char mode=0; char *str_mode = "w";
static char *kwlist[] = {"name","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|s", kwlist, &name,&str_mode))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Resolver object no longer valid");
// Precall preparations

if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

// Make the call
{
       Object returned_object = (Object)((Resolver)self->base)->create(((Resolver)self->base), name, mode);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Resolver.resolve_value(RDFURN uri ,char * attribute ,RDFValue value );
********************************************************/

static PyObject *pyResolver_resolve_value(pyResolver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
Gen_wrapper *uri;
char * attribute;
Gen_wrapper *value;
static char *kwlist[] = {"uri","attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "OsO|", kwlist, &uri,&attribute,&value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Resolver object no longer valid");
// Precall preparations

// Make the call
func_return = ((Resolver)self->base)->resolve_value(((Resolver)self->base), uri->base, attribute, value->base);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
RESOLVER_ITER Resolver.get_iter(void * None ,RDFURN uri ,char * attribute );
********************************************************/

static PyObject *pyResolver_get_iter(pyResolver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
Gen_wrapper *uri;
char * attribute;
static char *kwlist[] = {"uri","attribute", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|", kwlist, &uri,&attribute))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Resolver object no longer valid");
// Precall preparations

// Make the call

func_return = (pyRESOLVER_ITER *)PyObject_New(pyRESOLVER_ITER, &RESOLVER_ITER_Type);
func_return->base = ((Resolver)self->base)->get_iter(((Resolver)self->base), NULL, uri->base, attribute);

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Resolver.iter_next(RESOLVER_ITER iter ,RDFValue result );
********************************************************/

static PyObject *pyResolver_iter_next(pyResolver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
Gen_wrapper *iter;
Gen_wrapper *result;
static char *kwlist[] = {"iter","result", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "OO|", kwlist, &iter,&result))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Resolver object no longer valid");
// Precall preparations

// Make the call
func_return = ((Resolver)self->base)->iter_next(((Resolver)self->base), iter->base, result->base);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
RDFValue Resolver.iter_next_alloc(RESOLVER_ITER iter );
********************************************************/

static PyObject *pyResolver_iter_next_alloc(pyResolver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
Gen_wrapper *iter;
static char *kwlist[] = {"iter", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|", kwlist, &iter))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Resolver object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((Resolver)self->base)->iter_next_alloc(((Resolver)self->base), iter->base);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFValue: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * Resolver.del(RDFURN uri ,char * attribute );
********************************************************/

static PyObject *pyResolver_del(pyResolver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *uri;
char * attribute;
static char *kwlist[] = {"uri","attribute", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|", kwlist, &uri,&attribute))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Resolver object no longer valid");
// Precall preparations

// Make the call
((Resolver)self->base)->del(((Resolver)self->base), uri->base, attribute);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * Resolver.set_value(RDFURN uri ,char * attribute ,RDFValue value );
********************************************************/

static PyObject *pyResolver_set_value(pyResolver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *uri;
char * attribute;
Gen_wrapper *value;
static char *kwlist[] = {"uri","attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "OsO|", kwlist, &uri,&attribute,&value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Resolver object no longer valid");
// Precall preparations

// Make the call
((Resolver)self->base)->set_value(((Resolver)self->base), uri->base, attribute, value->base);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * Resolver.add_value(RDFURN uri ,char * attribute ,RDFValue value );
********************************************************/

static PyObject *pyResolver_add_value(pyResolver *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *uri;
char * attribute;
Gen_wrapper *value;
static char *kwlist[] = {"uri","attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "OsO|", kwlist, &uri,&attribute,&value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "Resolver object no longer valid");
// Precall preparations

// Make the call
((Resolver)self->base)->add_value(((Resolver)self->base), uri->base, attribute, value->base);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


static PyTypeObject Resolver_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.Resolver",               /* tp_name */
    sizeof(pyResolver),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)Resolver_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)Resolver_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "Resolver:  The resolver is at the heart of the AFF4 specification - its\n    responsible for returning objects keyed by attribute from a\n    globally unique identifier (URI) and managing the central\n    information store.\n",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Resolver_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyResolver_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef FileBackedObject_methods[] = {
     {"set_property",(PyCFunction)pyFileBackedObject_set_property, METH_VARARGS|METH_KEYWORDS, "void * FileBackedObject.set_property(char * attribute ,RDFValue value );\n\n\n"},
     {"finish",(PyCFunction)pyFileBackedObject_finish, METH_VARARGS|METH_KEYWORDS, "AFFObject FileBackedObject.finish();\n\n\n"},
     {"seek",(PyCFunction)pyFileBackedObject_seek, METH_VARARGS|METH_KEYWORDS, "uint64_t  FileBackedObject.seek(uint64_t  offset ,uint64_t  whence );\n\n\n"},
     {"read",(PyCFunction)pyFileBackedObject_read, METH_VARARGS|METH_KEYWORDS, "uint64_t  FileBackedObject.read(OUT char * buffer, unsigned long int length);\n\n\n"},
     {"write",(PyCFunction)pyFileBackedObject_write, METH_VARARGS|METH_KEYWORDS, "uint64_t  FileBackedObject.write(char * buffer, unsigned long int length);\n\n\n"},
     {"tell",(PyCFunction)pyFileBackedObject_tell, METH_VARARGS|METH_KEYWORDS, "uint64_t  FileBackedObject.tell();\n\n\n"},
     {"get_data",(PyCFunction)pyFileBackedObject_get_data, METH_VARARGS|METH_KEYWORDS, "char * FileBackedObject.get_data();\n\n\n"},
     {"truncate",(PyCFunction)pyFileBackedObject_truncate, METH_VARARGS|METH_KEYWORDS, "uint64_t  FileBackedObject.truncate(uint64_t  offset );\n\n\n"},
     {"close",(PyCFunction)pyFileBackedObject_close, METH_VARARGS|METH_KEYWORDS, "void * FileBackedObject.close();\n\n\n"},
     {NULL}  /* Sentinel */
};
static void
FileBackedObject_dealloc(pyFileBackedObject *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyFileBackedObject_init(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
Gen_wrapper *urn;
char mode=0; char *str_mode = "\x0";
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|", kwlist, &urn,&str_mode))
 goto error;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

self->base = CONSTRUCT(FileBackedObject, AFFObject, Con, NULL, urn->base, mode);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class FileBackedObject");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *FileBackedObject_getattr(pyFileBackedObject *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("readptr");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("data");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("fd");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=FileBackedObject_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "urn")) {
    PyObject *py_result;
    Gen_wrapper *urn;


    {
       Object returned_object = (Object)(((AFFObject)self->base)->urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       urn = new_class_wrapper(returned_object);
       if(!urn) goto error;
    }
talloc_increase_ref_count(urn->base);

    py_result = (PyObject *)urn;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((AFFObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "readptr")) {
    PyObject *py_result;
    uint64_t  readptr;


    readptr = (((FileLikeObject)self->base)->readptr);

    py_result = PyLong_FromLong(readptr);

    return py_result;
};
if(!strcmp(name, "size")) {
    PyObject *py_result;
    Gen_wrapper *size;


    {
       Object returned_object = (Object)(((FileLikeObject)self->base)->size);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       size = new_class_wrapper(returned_object);
       if(!size) goto error;
    }
talloc_increase_ref_count(size->base);

    py_result = (PyObject *)size;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((FileLikeObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "data")) {
    PyObject *py_result;
    char * data;


    data = (((FileLikeObject)self->base)->data);

    py_result = PyString_FromStringAndSize((char *)data, strlen(data));

    return py_result;
};
if(!strcmp(name, "fd")) {
    PyObject *py_result;
    uint64_t  fd;


    fd = (((FileBackedObject)self->base)->fd);

    py_result = PyLong_FromLong(fd);

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
void * FileBackedObject.set_property(char * attribute ,RDFValue value );
********************************************************/

static PyObject *pyFileBackedObject_set_property(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * attribute;
Gen_wrapper *value;
static char *kwlist[] = {"attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "sO|", kwlist, &attribute,&value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileBackedObject object no longer valid");
// Precall preparations

// Make the call
((AFFObject)self->base)->set_property(((AFFObject)self->base), attribute, value->base);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
AFFObject FileBackedObject.finish();
********************************************************/

static PyObject *pyFileBackedObject_finish(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileBackedObject object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((AFFObject)self->base)->finish(((AFFObject)self->base));

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations
self->base = NULL;

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    self->base = NULL;
return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileBackedObject.seek(uint64_t  offset ,uint64_t  whence );
********************************************************/

static PyObject *pyFileBackedObject_seek(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
uint64_t  whence;
static char *kwlist[] = {"offset","whence", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "KK|", kwlist, &offset,&whence))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileBackedObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->seek(((FileLikeObject)self->base), offset, whence);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileBackedObject.read(OUT char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyFileBackedObject_read(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
PyObject *tmp_buffer;
static char *kwlist[] = {"length", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "l|", kwlist, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileBackedObject object no longer valid");
// Precall preparations
tmp_buffer = PyString_FromStringAndSize(NULL, length);
PyString_AsStringAndSize(tmp_buffer, &buffer, (Py_ssize_t *)&length);

// Make the call
func_return = ((FileLikeObject)self->base)->read(((FileLikeObject)self->base), (OUT char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
returned_result = PyList_New(0);
py_result = PyLong_FromLong(func_return);
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
 _PyString_Resize(&tmp_buffer, func_return); 
py_result = tmp_buffer;
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileBackedObject.write(char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyFileBackedObject_write(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"buffer", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &buffer, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileBackedObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->write(((FileLikeObject)self->base), (char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileBackedObject.tell();
********************************************************/

static PyObject *pyFileBackedObject_tell(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileBackedObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->tell(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
char * FileBackedObject.get_data();
********************************************************/

static PyObject *pyFileBackedObject_get_data(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileBackedObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->get_data(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileBackedObject.truncate(uint64_t  offset );
********************************************************/

static PyObject *pyFileBackedObject_truncate(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
static char *kwlist[] = {"offset", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "K|", kwlist, &offset))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileBackedObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->truncate(((FileLikeObject)self->base), offset);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * FileBackedObject.close();
********************************************************/

static PyObject *pyFileBackedObject_close(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileBackedObject object no longer valid");
// Precall preparations

// Make the call
((FileLikeObject)self->base)->close(((FileLikeObject)self->base));

// Postcall preparations
self->base = NULL;

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

};


static PyTypeObject FileBackedObject_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.FileBackedObject",               /* tp_name */
    sizeof(pyFileBackedObject),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)FileBackedObject_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)FileBackedObject_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "FileBackedObject:  This file like object is backed by a real disk file:\n",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    FileBackedObject_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyFileBackedObject_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef FileLikeObject_methods[] = {
     {"set_property",(PyCFunction)pyFileLikeObject_set_property, METH_VARARGS|METH_KEYWORDS, "void * FileLikeObject.set_property(char * attribute ,RDFValue value );\n\n\n"},
     {"finish",(PyCFunction)pyFileLikeObject_finish, METH_VARARGS|METH_KEYWORDS, "AFFObject FileLikeObject.finish();\n\n\n"},
     {"seek",(PyCFunction)pyFileLikeObject_seek, METH_VARARGS|METH_KEYWORDS, "uint64_t  FileLikeObject.seek(uint64_t  offset ,uint64_t  whence );\n\n\n"},
     {"read",(PyCFunction)pyFileLikeObject_read, METH_VARARGS|METH_KEYWORDS, "uint64_t  FileLikeObject.read(OUT char * buffer, unsigned long int length);\n\n\n"},
     {"write",(PyCFunction)pyFileLikeObject_write, METH_VARARGS|METH_KEYWORDS, "uint64_t  FileLikeObject.write(char * buffer, unsigned long int length);\n\n\n"},
     {"tell",(PyCFunction)pyFileLikeObject_tell, METH_VARARGS|METH_KEYWORDS, "uint64_t  FileLikeObject.tell();\n\n\n"},
     {"get_data",(PyCFunction)pyFileLikeObject_get_data, METH_VARARGS|METH_KEYWORDS, "char * FileLikeObject.get_data();\n\n\n This can be used to get the content of the FileLikeObject in a\n big buffer of data. The data will be cached with the\n FileLikeObject. Its only really suitable for smallish amounts of\n data - and checks to ensure that file size is less than MAX_CACHED_FILESIZE\n"},
     {"truncate",(PyCFunction)pyFileLikeObject_truncate, METH_VARARGS|METH_KEYWORDS, "uint64_t  FileLikeObject.truncate(uint64_t  offset );\n\n\n This method is just like the standard ftruncate call\n"},
     {"close",(PyCFunction)pyFileLikeObject_close, METH_VARARGS|METH_KEYWORDS, "void * FileLikeObject.close();\n\n\n This closes the FileLikeObject and also frees it - it is not valid\n to use the FileLikeObject after calling this (it gets free\'d).\n"},
     {NULL}  /* Sentinel */
};
static void
FileLikeObject_dealloc(pyFileLikeObject *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyFileLikeObject_init(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
Gen_wrapper *urn;
char mode=0; char *str_mode = "\x0";
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|", kwlist, &urn,&str_mode))
 goto error;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  goto error;
};

mode = str_mode[0];

self->base = CONSTRUCT(FileLikeObject, AFFObject, Con, NULL, urn->base, mode);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class FileLikeObject");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *FileLikeObject_getattr(pyFileLikeObject *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("urn");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("readptr");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("size");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("mode");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("data");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=FileLikeObject_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "urn")) {
    PyObject *py_result;
    Gen_wrapper *urn;


    {
       Object returned_object = (Object)(((AFFObject)self->base)->urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       urn = new_class_wrapper(returned_object);
       if(!urn) goto error;
    }
talloc_increase_ref_count(urn->base);

    py_result = (PyObject *)urn;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((AFFObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "readptr")) {
    PyObject *py_result;
    uint64_t  readptr;


    readptr = (((FileLikeObject)self->base)->readptr);

    py_result = PyLong_FromLong(readptr);

    return py_result;
};
if(!strcmp(name, "size")) {
    PyObject *py_result;
    Gen_wrapper *size;


    {
       Object returned_object = (Object)(((FileLikeObject)self->base)->size);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object XSDInteger: %s", __error_str);
         ClearError();
         goto error;
       };

       size = new_class_wrapper(returned_object);
       if(!size) goto error;
    }
talloc_increase_ref_count(size->base);

    py_result = (PyObject *)size;

    return py_result;
};
if(!strcmp(name, "mode")) {
    PyObject *py_result;
    char mode=0; char *str_mode = "\x0";


    mode = (((FileLikeObject)self->base)->mode);

    str_mode = &mode;
    py_result = PyString_FromStringAndSize(str_mode, 1);

    return py_result;
};
if(!strcmp(name, "data")) {
    PyObject *py_result;
    char * data;


    data = (((FileLikeObject)self->base)->data);

    py_result = PyString_FromStringAndSize((char *)data, strlen(data));

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
void * FileLikeObject.set_property(char * attribute ,RDFValue value );
********************************************************/

static PyObject *pyFileLikeObject_set_property(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * attribute;
Gen_wrapper *value;
static char *kwlist[] = {"attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "sO|", kwlist, &attribute,&value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileLikeObject object no longer valid");
// Precall preparations

// Make the call
((AFFObject)self->base)->set_property(((AFFObject)self->base), attribute, value->base);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
AFFObject FileLikeObject.finish();
********************************************************/

static PyObject *pyFileLikeObject_finish(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileLikeObject object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((AFFObject)self->base)->finish(((AFFObject)self->base));

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations
self->base = NULL;

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    self->base = NULL;
return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileLikeObject.seek(uint64_t  offset ,uint64_t  whence );
********************************************************/

static PyObject *pyFileLikeObject_seek(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
uint64_t  whence;
static char *kwlist[] = {"offset","whence", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "KK|", kwlist, &offset,&whence))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileLikeObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->seek(((FileLikeObject)self->base), offset, whence);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileLikeObject.read(OUT char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyFileLikeObject_read(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
PyObject *tmp_buffer;
static char *kwlist[] = {"length", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "l|", kwlist, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileLikeObject object no longer valid");
// Precall preparations
tmp_buffer = PyString_FromStringAndSize(NULL, length);
PyString_AsStringAndSize(tmp_buffer, &buffer, (Py_ssize_t *)&length);

// Make the call
func_return = ((FileLikeObject)self->base)->read(((FileLikeObject)self->base), (OUT char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
returned_result = PyList_New(0);
py_result = PyLong_FromLong(func_return);
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
 _PyString_Resize(&tmp_buffer, func_return); 
py_result = tmp_buffer;
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileLikeObject.write(char * buffer, unsigned long int length);
********************************************************/

static PyObject *pyFileLikeObject_write(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *buffer=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"buffer", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &buffer, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileLikeObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->write(((FileLikeObject)self->base), (char *)buffer, (unsigned long int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileLikeObject.tell();
********************************************************/

static PyObject *pyFileLikeObject_tell(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileLikeObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->tell(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
char * FileLikeObject.get_data();
********************************************************/

static PyObject *pyFileLikeObject_get_data(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileLikeObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->get_data(((FileLikeObject)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileLikeObject.truncate(uint64_t  offset );
********************************************************/

static PyObject *pyFileLikeObject_truncate(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
static char *kwlist[] = {"offset", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "K|", kwlist, &offset))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileLikeObject object no longer valid");
// Precall preparations

// Make the call
func_return = ((FileLikeObject)self->base)->truncate(((FileLikeObject)self->base), offset);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * FileLikeObject.close();
********************************************************/

static PyObject *pyFileLikeObject_close(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "FileLikeObject object no longer valid");
// Precall preparations

// Make the call
((FileLikeObject)self->base)->close(((FileLikeObject)self->base));

// Postcall preparations
self->base = NULL;

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

};


static PyTypeObject FileLikeObject_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.FileLikeObject",               /* tp_name */
    sizeof(pyFileLikeObject),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)FileLikeObject_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)FileLikeObject_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "FileLikeObject: ",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    FileLikeObject_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyFileLikeObject_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef RESOLVER_ITER_methods[] = {
     {NULL}  /* Sentinel */
};
static void
RESOLVER_ITER_dealloc(pyRESOLVER_ITER *self) {
if(self->base) talloc_free(self->base);
};

static int pyRESOLVER_ITER_init(pyRESOLVER_ITER *self, PyObject *args, PyObject *kwds) {

self->base = NULL;
  return 0;
};


static PyObject *RESOLVER_ITER_getattr(pyRESOLVER_ITER *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("offset");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=RESOLVER_ITER_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "offset")) {
    PyObject *py_result;
    uint64_t  offset;


    offset = (self->base->offset);

    py_result = PyLong_FromLong(offset);

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


static PyTypeObject RESOLVER_ITER_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.RESOLVER_ITER",               /* tp_name */
    sizeof(pyRESOLVER_ITER),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)RESOLVER_ITER_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)RESOLVER_ITER_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "RESOLVER_ITER:  This object is returned when iterating the a result set from the\n    resolver. Its basically a pointer into the resolver data store.",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    RESOLVER_ITER_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyRESOLVER_ITER_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef XSDInteger_methods[] = {
     {"parse",(PyCFunction)pyXSDInteger_parse, METH_VARARGS|METH_KEYWORDS, "uint64_t  XSDInteger.parse(char * serialised_form );\n\n\n"},
     {"encode",(PyCFunction)pyXSDInteger_encode, METH_VARARGS|METH_KEYWORDS, "TDB_DATA * XSDInteger.encode();\n\n\n"},
     {"decode",(PyCFunction)pyXSDInteger_decode, METH_VARARGS|METH_KEYWORDS, "uint64_t  XSDInteger.decode(char * data, int length);\n\n\n"},
     {"serialise",(PyCFunction)pyXSDInteger_serialise, METH_VARARGS|METH_KEYWORDS, "char * XSDInteger.serialise();\n\n\n"},
     {"set",(PyCFunction)pyXSDInteger_set, METH_VARARGS|METH_KEYWORDS, "RDFValue XSDInteger.set(uint64_t  value );\n\n\n"},
     {"get",(PyCFunction)pyXSDInteger_get, METH_VARARGS|METH_KEYWORDS, "uint64_t  XSDInteger.get();\n\n\n"},
     {NULL}  /* Sentinel */
};
static void
XSDInteger_dealloc(pyXSDInteger *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyXSDInteger_init(pyXSDInteger *self, PyObject *args, PyObject *kwds) {

self->base = CONSTRUCT(XSDInteger, RDFValue, Con, NULL);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class XSDInteger");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *XSDInteger_getattr(pyXSDInteger *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("dataType");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("id");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("value");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("serialised");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=XSDInteger_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "dataType")) {
    PyObject *py_result;
    char * dataType;


    dataType = (((RDFValue)self->base)->dataType);

    py_result = PyString_FromStringAndSize((char *)dataType, strlen(dataType));

    return py_result;
};
if(!strcmp(name, "id")) {
    PyObject *py_result;
    uint64_t  id;


    id = (((RDFValue)self->base)->id);

    py_result = PyLong_FromLong(id);

    return py_result;
};
if(!strcmp(name, "value")) {
    PyObject *py_result;
    uint64_t  value;


    value = (((XSDInteger)self->base)->value);

    py_result = PyLong_FromLong(value);

    return py_result;
};
if(!strcmp(name, "serialised")) {
    PyObject *py_result;
    char * serialised;


    serialised = (((XSDInteger)self->base)->serialised);

    py_result = PyString_FromStringAndSize((char *)serialised, strlen(serialised));

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDInteger.parse(char * serialised_form );
********************************************************/

static PyObject *pyXSDInteger_parse(pyXSDInteger *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char * serialised_form;
static char *kwlist[] = {"serialised_form", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &serialised_form))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDInteger object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->parse(((RDFValue)self->base), serialised_form);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA * XSDInteger.encode();
********************************************************/

static PyObject *pyXSDInteger_encode(pyXSDInteger *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
TDB_DATA * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDInteger object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->encode(((RDFValue)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return->dptr, func_return->dsize);
talloc_free(func_return);returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDInteger.decode(char * data, int length);
********************************************************/

static PyObject *pyXSDInteger_decode(pyXSDInteger *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *data=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"data", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &data, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDInteger object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->decode(((RDFValue)self->base), (char *)data, (int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
char * XSDInteger.serialise();
********************************************************/

static PyObject *pyXSDInteger_serialise(pyXSDInteger *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDInteger object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->serialise(((RDFValue)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
RDFValue XSDInteger.set(uint64_t  value );
********************************************************/

static PyObject *pyXSDInteger_set(pyXSDInteger *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
uint64_t  value;
static char *kwlist[] = {"value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "K|", kwlist, &value))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDInteger object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((XSDInteger)self->base)->set(((XSDInteger)self->base), value);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFValue: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }
talloc_increase_ref_count(func_return->base);

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDInteger.get();
********************************************************/

static PyObject *pyXSDInteger_get(pyXSDInteger *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "XSDInteger object no longer valid");
// Precall preparations

// Make the call
func_return = ((XSDInteger)self->base)->get(((XSDInteger)self->base));

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

};


static PyTypeObject XSDInteger_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.XSDInteger",               /* tp_name */
    sizeof(pyXSDInteger),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)XSDInteger_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)XSDInteger_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "XSDInteger:  An integer encoded according the XML RFC. ",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    XSDInteger_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyXSDInteger_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
static PyMethodDef RDFURN_methods[] = {
     {"parse",(PyCFunction)pyRDFURN_parse, METH_VARARGS|METH_KEYWORDS, "uint64_t  RDFURN.parse(char * serialised_form );\n\n\n"},
     {"encode",(PyCFunction)pyRDFURN_encode, METH_VARARGS|METH_KEYWORDS, "TDB_DATA * RDFURN.encode();\n\n\n"},
     {"decode",(PyCFunction)pyRDFURN_decode, METH_VARARGS|METH_KEYWORDS, "uint64_t  RDFURN.decode(char * data, int length);\n\n\n"},
     {"serialise",(PyCFunction)pyRDFURN_serialise, METH_VARARGS|METH_KEYWORDS, "char * RDFURN.serialise();\n\n\n"},
     {"set",(PyCFunction)pyRDFURN_set, METH_VARARGS|METH_KEYWORDS, "RDFValue RDFURN.set(char * urn );\n\n\n"},
     {"get",(PyCFunction)pyRDFURN_get, METH_VARARGS|METH_KEYWORDS, "char * RDFURN.get();\n\n\n"},
     {"copy",(PyCFunction)pyRDFURN_copy, METH_VARARGS|METH_KEYWORDS, "RDFURN RDFURN.copy(void * None );\n\n\n Make a new RDFURN as a copy of this one\n"},
     {"add",(PyCFunction)pyRDFURN_add, METH_VARARGS|METH_KEYWORDS, "void * RDFURN.add(char * urn );\n\n\n Add a relative stem to the current value. If urn is a fully\n qualified URN, we replace the current value with it\n"},
     {"relative_name",(PyCFunction)pyRDFURN_relative_name, METH_VARARGS|METH_KEYWORDS, "TDB_DATA RDFURN.relative_name(RDFURN volume );\n\n\n This method returns the relative name\n"},
     {NULL}  /* Sentinel */
};
static void
RDFURN_dealloc(pyRDFURN *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyRDFURN_init(pyRDFURN *self, PyObject *args, PyObject *kwds) {

self->base = CONSTRUCT(RDFURN, RDFValue, Con, NULL);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class RDFURN");
    goto error;
  };
  return 0;
error:
    return -1;
};


static PyObject *RDFURN_getattr(pyRDFURN *self, PyObject *pyname) {
  char *name = PyString_AsString(pyname);

  if(!name) return NULL;
if(!strcmp(name, "__members__")) {
     PyObject *result = PyList_New(0);
     PyObject *tmp;
     PyMethodDef *i;

     if(!result) goto error;
 tmp = PyString_FromString("dataType");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("id");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("value");
    PyList_Append(result, tmp); Py_DECREF(tmp);
 tmp = PyString_FromString("parser");
    PyList_Append(result, tmp); Py_DECREF(tmp);


    for(i=RDFURN_methods; i->ml_name; i++) {
     tmp = PyString_FromString(i->ml_name);
    PyList_Append(result, tmp); Py_DECREF(tmp);
    }; 
     return result; 
   }

if(!strcmp(name, "dataType")) {
    PyObject *py_result;
    char * dataType;


    dataType = (((RDFValue)self->base)->dataType);

    py_result = PyString_FromStringAndSize((char *)dataType, strlen(dataType));

    return py_result;
};
if(!strcmp(name, "id")) {
    PyObject *py_result;
    uint64_t  id;


    id = (((RDFValue)self->base)->id);

    py_result = PyLong_FromLong(id);

    return py_result;
};
if(!strcmp(name, "value")) {
    PyObject *py_result;
    char * value;


    value = (((RDFURN)self->base)->value);

    py_result = PyString_FromStringAndSize((char *)value, strlen(value));

    return py_result;
};
if(!strcmp(name, "parser")) {
    PyObject *py_result;
    Gen_wrapper *parser;


    {
       Object returned_object = (Object)(((RDFURN)self->base)->parser);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object URLParse: %s", __error_str);
         ClearError();
         goto error;
       };

       parser = new_class_wrapper(returned_object);
       if(!parser) goto error;
    }
talloc_increase_ref_count(parser->base);

    py_result = (PyObject *)parser;

    return py_result;
};

  // Hand it off to the python native handler
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
error:
return NULL;
}


/********************************************************
Autogenerated wrapper for function:
uint64_t  RDFURN.parse(char * serialised_form );
********************************************************/

static PyObject *pyRDFURN_parse(pyRDFURN *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char * serialised_form;
static char *kwlist[] = {"serialised_form", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &serialised_form))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFURN object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->parse(((RDFValue)self->base), serialised_form);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA * RDFURN.encode();
********************************************************/

static PyObject *pyRDFURN_encode(pyRDFURN *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
TDB_DATA * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFURN object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->encode(((RDFValue)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return->dptr, func_return->dsize);
talloc_free(func_return);returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
uint64_t  RDFURN.decode(char * data, int length);
********************************************************/

static PyObject *pyRDFURN_decode(pyRDFURN *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
uint64_t  func_return;
char *data=""; Py_ssize_t length=strlen("");
static char *kwlist[] = {"data", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#|", kwlist, &data, &length))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFURN object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->decode(((RDFValue)self->base), (char *)data, (int)length);

// Postcall preparations

// prepare results
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
char * RDFURN.serialise();
********************************************************/

static PyObject *pyRDFURN_serialise(pyRDFURN *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFURN object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFValue)self->base)->serialise(((RDFValue)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
RDFValue RDFURN.set(char * urn );
********************************************************/

static PyObject *pyRDFURN_set(pyRDFURN *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
char * urn;
static char *kwlist[] = {"urn", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &urn))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFURN object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((RDFURN)self->base)->set(((RDFURN)self->base), urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFValue: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }
talloc_increase_ref_count(func_return->base);

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
char * RDFURN.get();
********************************************************/

static PyObject *pyRDFURN_get(pyRDFURN *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFURN object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFURN)self->base)->get(((RDFURN)self->base));

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
returned_result = py_result;
return returned_result;

};


/********************************************************
Autogenerated wrapper for function:
RDFURN RDFURN.copy(void * None );
********************************************************/

static PyObject *pyRDFURN_copy(pyRDFURN *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
Gen_wrapper *func_return;
// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFURN object no longer valid");
// Precall preparations

// Make the call
{
       Object returned_object = (Object)((RDFURN)self->base)->copy(((RDFURN)self->base), NULL);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN: %s", __error_str);
         ClearError();
         goto error;
       };

       func_return = new_class_wrapper(returned_object);
       if(!func_return) goto error;
    }

// Postcall preparations

// prepare results
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
void * RDFURN.add(char * urn );
********************************************************/

static PyObject *pyRDFURN_add(pyRDFURN *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
char * urn;
static char *kwlist[] = {"urn", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &urn))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFURN object no longer valid");
// Precall preparations

// Make the call
((RDFURN)self->base)->add(((RDFURN)self->base), urn);

// Postcall preparations

// prepare results
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA RDFURN.relative_name(RDFURN volume );
********************************************************/

static PyObject *pyRDFURN_relative_name(pyRDFURN *self, PyObject *args, PyObject *kwds) {
       PyObject *returned_result, *py_result;
TDB_DATA func_return;
Gen_wrapper *volume;
static char *kwlist[] = {"volume", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|", kwlist, &volume))
 goto error;

// Make sure that we have something valid to wrap
if(!self->base) return PyErr_Format(PyExc_RuntimeError, "RDFURN object no longer valid");
// Precall preparations

// Make the call
func_return = ((RDFURN)self->base)->relative_name(((RDFURN)self->base), volume->base);

// Postcall preparations

// prepare results
py_result = PyString_FromStringAndSize((char *)func_return.dptr, func_return.dsize);
returned_result = py_result;
return returned_result;

// error conditions:
error:
    return NULL;

};


static PyTypeObject RDFURN_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.RDFURN",               /* tp_name */
    sizeof(pyRDFURN),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)RDFURN_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    (getattrofunc)RDFURN_getattr,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "RDFURN:  A URN for use in the rest of the library ",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    RDFURN_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyRDFURN_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};

static PyMethodDef pyaff4_methods[] = {
     {NULL}  /* Sentinel */
};

PyMODINIT_FUNC initpyaff4(void) {
   /* create module */
   PyObject *m = Py_InitModule3("pyaff4", pyaff4_methods,
                                   "pyaff4 module.");
   PyObject *d = PyModule_GetDict(m);
   PyObject *tmp;

 MapDriver_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&MapDriver_Type) < 0)
     return;

 Py_INCREF((PyObject *)&MapDriver_Type);
 PyModule_AddObject(m, "MapDriver", (PyObject *)&MapDriver_Type);

 AES256Password_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&AES256Password_Type) < 0)
     return;

 Py_INCREF((PyObject *)&AES256Password_Type);
 PyModule_AddObject(m, "AES256Password", (PyObject *)&AES256Password_Type);

 XSDString_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&XSDString_Type) < 0)
     return;

 Py_INCREF((PyObject *)&XSDString_Type);
 PyModule_AddObject(m, "XSDString", (PyObject *)&XSDString_Type);

 AFFObject_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&AFFObject_Type) < 0)
     return;

 Py_INCREF((PyObject *)&AFFObject_Type);
 PyModule_AddObject(m, "AFFObject", (PyObject *)&AFFObject_Type);

 Image_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&Image_Type) < 0)
     return;

 Py_INCREF((PyObject *)&Image_Type);
 PyModule_AddObject(m, "Image", (PyObject *)&Image_Type);

 Encrypted_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&Encrypted_Type) < 0)
     return;

 Py_INCREF((PyObject *)&Encrypted_Type);
 PyModule_AddObject(m, "Encrypted", (PyObject *)&Encrypted_Type);

 ZipFile_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&ZipFile_Type) < 0)
     return;

 Py_INCREF((PyObject *)&ZipFile_Type);
 PyModule_AddObject(m, "ZipFile", (PyObject *)&ZipFile_Type);

 XSDDatetime_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&XSDDatetime_Type) < 0)
     return;

 Py_INCREF((PyObject *)&XSDDatetime_Type);
 PyModule_AddObject(m, "XSDDatetime", (PyObject *)&XSDDatetime_Type);

 URLParse_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&URLParse_Type) < 0)
     return;

 Py_INCREF((PyObject *)&URLParse_Type);
 PyModule_AddObject(m, "URLParse", (PyObject *)&URLParse_Type);

 RDFValue_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&RDFValue_Type) < 0)
     return;

 Py_INCREF((PyObject *)&RDFValue_Type);
 PyModule_AddObject(m, "RDFValue", (PyObject *)&RDFValue_Type);

 HTTPObject_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&HTTPObject_Type) < 0)
     return;

 Py_INCREF((PyObject *)&HTTPObject_Type);
 PyModule_AddObject(m, "HTTPObject", (PyObject *)&HTTPObject_Type);

 ZipFileStream_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&ZipFileStream_Type) < 0)
     return;

 Py_INCREF((PyObject *)&ZipFileStream_Type);
 PyModule_AddObject(m, "ZipFileStream", (PyObject *)&ZipFileStream_Type);

 Resolver_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&Resolver_Type) < 0)
     return;

 Py_INCREF((PyObject *)&Resolver_Type);
 PyModule_AddObject(m, "Resolver", (PyObject *)&Resolver_Type);

 FileBackedObject_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&FileBackedObject_Type) < 0)
     return;

 Py_INCREF((PyObject *)&FileBackedObject_Type);
 PyModule_AddObject(m, "FileBackedObject", (PyObject *)&FileBackedObject_Type);

 FileLikeObject_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&FileLikeObject_Type) < 0)
     return;

 Py_INCREF((PyObject *)&FileLikeObject_Type);
 PyModule_AddObject(m, "FileLikeObject", (PyObject *)&FileLikeObject_Type);

 RESOLVER_ITER_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&RESOLVER_ITER_Type) < 0)
     return;

 Py_INCREF((PyObject *)&RESOLVER_ITER_Type);
 PyModule_AddObject(m, "RESOLVER_ITER", (PyObject *)&RESOLVER_ITER_Type);

 XSDInteger_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&XSDInteger_Type) < 0)
     return;

 Py_INCREF((PyObject *)&XSDInteger_Type);
 PyModule_AddObject(m, "XSDInteger", (PyObject *)&XSDInteger_Type);

 RDFURN_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&RDFURN_Type) < 0)
     return;

 Py_INCREF((PyObject *)&RDFURN_Type);
 PyModule_AddObject(m, "RDFURN", (PyObject *)&RDFURN_Type);
 tmp = PyString_FromString(NAMESPACE); 

 PyDict_SetItemString(d, "NAMESPACE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(PREDICATE_NAMESPACE); 

 PyDict_SetItemString(d, "PREDICATE_NAMESPACE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(RDF_NAMESPACE); 

 PyDict_SetItemString(d, "RDF_NAMESPACE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(XSD_NAMESPACE); 

 PyDict_SetItemString(d, "XSD_NAMESPACE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(VOLATILE_NS); 

 PyDict_SetItemString(d, "VOLATILE_NS", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(CONFIGURATION_NS); 

 PyDict_SetItemString(d, "CONFIGURATION_NS", tmp);
 Py_DECREF(tmp);
 tmp = PyLong_FromUnsignedLongLong(FQN); 

 PyDict_SetItemString(d, "FQN", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(GLOBAL); 

 PyDict_SetItemString(d, "GLOBAL", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(CONFIG_THREADS); 

 PyDict_SetItemString(d, "CONFIG_THREADS", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(CONFIG_VERBOSE); 

 PyDict_SetItemString(d, "CONFIG_VERBOSE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(CONFIG_AUTOLOAD); 

 PyDict_SetItemString(d, "CONFIG_AUTOLOAD", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(CONFIG_PAD); 

 PyDict_SetItemString(d, "CONFIG_PAD", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_STORED); 

 PyDict_SetItemString(d, "AFF4_STORED", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_TYPE); 

 PyDict_SetItemString(d, "AFF4_TYPE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_INTERFACE); 

 PyDict_SetItemString(d, "AFF4_INTERFACE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_SIZE); 

 PyDict_SetItemString(d, "AFF4_SIZE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_SHA); 

 PyDict_SetItemString(d, "AFF4_SHA", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_TIMESTAMP); 

 PyDict_SetItemString(d, "AFF4_TIMESTAMP", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_STREAM); 

 PyDict_SetItemString(d, "AFF4_STREAM", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_VOLUME); 

 PyDict_SetItemString(d, "AFF4_VOLUME", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_VOLATILE_HEADER_OFFSET); 

 PyDict_SetItemString(d, "AFF4_VOLATILE_HEADER_OFFSET", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_VOLATILE_COMPRESSED_SIZE); 

 PyDict_SetItemString(d, "AFF4_VOLATILE_COMPRESSED_SIZE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_VOLATILE_CRC); 

 PyDict_SetItemString(d, "AFF4_VOLATILE_CRC", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_VOLATILE_COMPRESSION); 

 PyDict_SetItemString(d, "AFF4_VOLATILE_COMPRESSION", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_VOLATILE_FILE_OFFSET); 

 PyDict_SetItemString(d, "AFF4_VOLATILE_FILE_OFFSET", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_VOLATILE_DIRTY); 

 PyDict_SetItemString(d, "AFF4_VOLATILE_DIRTY", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_VOLATILE_CONTAINS); 

 PyDict_SetItemString(d, "AFF4_VOLATILE_CONTAINS", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CHUNK_SIZE); 

 PyDict_SetItemString(d, "AFF4_CHUNK_SIZE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_COMPRESSION); 

 PyDict_SetItemString(d, "AFF4_COMPRESSION", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CHUNKS_IN_SEGMENT); 

 PyDict_SetItemString(d, "AFF4_CHUNKS_IN_SEGMENT", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_DIRECTORY_OFFSET); 

 PyDict_SetItemString(d, "AFF4_DIRECTORY_OFFSET", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_TARGET); 

 PyDict_SetItemString(d, "AFF4_TARGET", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_BLOCKSIZE); 

 PyDict_SetItemString(d, "AFF4_BLOCKSIZE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_IMAGE_PERIOD); 

 PyDict_SetItemString(d, "AFF4_IMAGE_PERIOD", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_TARGET_PERIOD); 

 PyDict_SetItemString(d, "AFF4_TARGET_PERIOD", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_STATEMENT); 

 PyDict_SetItemString(d, "AFF4_STATEMENT", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CERT); 

 PyDict_SetItemString(d, "AFF4_CERT", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_PRIV_KEY); 

 PyDict_SetItemString(d, "AFF4_PRIV_KEY", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_COMMON_NAME); 

 PyDict_SetItemString(d, "AFF4_COMMON_NAME", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_IDENTITY_PREFIX); 

 PyDict_SetItemString(d, "AFF4_IDENTITY_PREFIX", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_HASH_TYPE); 

 PyDict_SetItemString(d, "AFF4_HASH_TYPE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_HIGHLIGHT); 

 PyDict_SetItemString(d, "AFF4_HIGHLIGHT", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_INFORMATION); 

 PyDict_SetItemString(d, "AFF4_INFORMATION", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_VOLATILE_PASSPHRASE); 

 PyDict_SetItemString(d, "AFF4_VOLATILE_PASSPHRASE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_VOLATILE_KEY); 

 PyDict_SetItemString(d, "AFF4_VOLATILE_KEY", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CRYPTO_NAMESPACE); 

 PyDict_SetItemString(d, "AFF4_CRYPTO_NAMESPACE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CRYPTO_FORTIFICATION_COUNT); 

 PyDict_SetItemString(d, "AFF4_CRYPTO_FORTIFICATION_COUNT", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CRYPTO_IV); 

 PyDict_SetItemString(d, "AFF4_CRYPTO_IV", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CRYPTO_RSA); 

 PyDict_SetItemString(d, "AFF4_CRYPTO_RSA", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CRYPTO_PASSPHRASE_KEY); 

 PyDict_SetItemString(d, "AFF4_CRYPTO_PASSPHRASE_KEY", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CRYPTO_ALGORITHM); 

 PyDict_SetItemString(d, "AFF4_CRYPTO_ALGORITHM", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CRYPTO_BLOCKSIZE); 

 PyDict_SetItemString(d, "AFF4_CRYPTO_BLOCKSIZE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CRYPTO_NONCE); 

 PyDict_SetItemString(d, "AFF4_CRYPTO_NONCE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CRYPTO_ALGORITHM_AES_SHA254); 

 PyDict_SetItemString(d, "AFF4_CRYPTO_ALGORITHM_AES_SHA254", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_ZIP_VOLUME); 

 PyDict_SetItemString(d, "AFF4_ZIP_VOLUME", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_DIRECTORY_VOLUME); 

 PyDict_SetItemString(d, "AFF4_DIRECTORY_VOLUME", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_LIBAFF_VOLUME); 

 PyDict_SetItemString(d, "AFF4_LIBAFF_VOLUME", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_LIBEWF_VOLUME); 

 PyDict_SetItemString(d, "AFF4_LIBEWF_VOLUME", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_SEGMENT); 

 PyDict_SetItemString(d, "AFF4_SEGMENT", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_LINK); 

 PyDict_SetItemString(d, "AFF4_LINK", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_IMAGE); 

 PyDict_SetItemString(d, "AFF4_IMAGE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_MAP); 

 PyDict_SetItemString(d, "AFF4_MAP", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_ENCRYTED); 

 PyDict_SetItemString(d, "AFF4_ENCRYTED", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_LIBAFF_STREAM); 

 PyDict_SetItemString(d, "AFF4_LIBAFF_STREAM", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_ERROR_STREAM); 

 PyDict_SetItemString(d, "AFF4_ERROR_STREAM", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_FILE); 

 PyDict_SetItemString(d, "AFF4_FILE", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_IDENTITY); 

 PyDict_SetItemString(d, "AFF4_IDENTITY", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_SPECIAL_URN_NULL); 

 PyDict_SetItemString(d, "AFF4_SPECIAL_URN_NULL", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_CIPHER); 

 PyDict_SetItemString(d, "AFF4_CIPHER", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_AES256_PASSWORD); 

 PyDict_SetItemString(d, "AFF4_AES256_PASSWORD", tmp);
 Py_DECREF(tmp);
 tmp = PyLong_FromUnsignedLongLong(MAX_CACHED_FILESIZE); 

 PyDict_SetItemString(d, "MAX_CACHED_FILESIZE", tmp);
 Py_DECREF(tmp);
 tmp = PyLong_FromUnsignedLongLong(AES256_KEY_SIZE); 

 PyDict_SetItemString(d, "AES256_KEY_SIZE", tmp);
 Py_DECREF(tmp);
 tmp = PyLong_FromUnsignedLongLong(ZIP64_LIMIT); 

 PyDict_SetItemString(d, "ZIP64_LIMIT", tmp);
 Py_DECREF(tmp);
 tmp = PyLong_FromUnsignedLongLong(ZIP_STORED); 

 PyDict_SetItemString(d, "ZIP_STORED", tmp);
 Py_DECREF(tmp);
 tmp = PyLong_FromUnsignedLongLong(ZIP_DEFLATE); 

 PyDict_SetItemString(d, "ZIP_DEFLATE", tmp);
 Py_DECREF(tmp);

//talloc_enable_leak_report_full();
AFF4_Init();

python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__MapDriver;
python_wrappers[TOTAL_CLASSES++].python_type = &MapDriver_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__AES256Password;
python_wrappers[TOTAL_CLASSES++].python_type = &AES256Password_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__XSDString;
python_wrappers[TOTAL_CLASSES++].python_type = &XSDString_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__AFFObject;
python_wrappers[TOTAL_CLASSES++].python_type = &AFFObject_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__Image;
python_wrappers[TOTAL_CLASSES++].python_type = &Image_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__Encrypted;
python_wrappers[TOTAL_CLASSES++].python_type = &Encrypted_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__ZipFile;
python_wrappers[TOTAL_CLASSES++].python_type = &ZipFile_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__XSDDatetime;
python_wrappers[TOTAL_CLASSES++].python_type = &XSDDatetime_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__URLParse;
python_wrappers[TOTAL_CLASSES++].python_type = &URLParse_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__RDFValue;
python_wrappers[TOTAL_CLASSES++].python_type = &RDFValue_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__HTTPObject;
python_wrappers[TOTAL_CLASSES++].python_type = &HTTPObject_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__ZipFileStream;
python_wrappers[TOTAL_CLASSES++].python_type = &ZipFileStream_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__Resolver;
python_wrappers[TOTAL_CLASSES++].python_type = &Resolver_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__FileBackedObject;
python_wrappers[TOTAL_CLASSES++].python_type = &FileBackedObject_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__FileLikeObject;
python_wrappers[TOTAL_CLASSES++].python_type = &FileLikeObject_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__XSDInteger;
python_wrappers[TOTAL_CLASSES++].python_type = &XSDInteger_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__RDFURN;
python_wrappers[TOTAL_CLASSES++].python_type = &RDFURN_Type;
}

