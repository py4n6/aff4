#include <Python.h>
#include "../include/aff4_constants.h"
#include "../include/aff4_rdf.h"
#include "../include/aff4_io.h"
#include "../include/aff4_resolver.h"

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
} python_wrappers[13];

/* Create the relevant wrapper from the item based on the lookup
table.
*/
PyObject *new_class_wrapper(Object item) {
   int i;
   PyObject *result;

   for(i=0; i<TOTAL_CLASSES; i++) {
     if(python_wrappers[i].class_ref == item->__class__) {
       result = _PyObject_New(python_wrappers[i].python_type);
       return result;
     };
   };

   return NULL;
};


typedef struct {
  PyObject_HEAD
  XSDString base;
} pyXSDString; 

staticforward PyTypeObject XSDString_Type;
static int pyXSDString_init(pyXSDString *self, PyObject *args, PyObject *kwds);
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
static PyObject *pyAFFObject_set_property(pyAFFObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyAFFObject_finish(pyAFFObject *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  XSDDatetime base;
} pyXSDDatetime; 

staticforward PyTypeObject XSDDatetime_Type;
static int pyXSDDatetime_init(pyXSDDatetime *self, PyObject *args, PyObject *kwds);
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
static PyObject *pyURLParse_parse(pyURLParse *self, PyObject *args, PyObject *kwds);
static PyObject *pyURLParse_string(pyURLParse *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  TDB_DATA_LIST *base;
} pyTDB_DATA_LIST; 

staticforward PyTypeObject TDB_DATA_LIST_Type;
static int pyTDB_DATA_LIST_init(pyTDB_DATA_LIST *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  RDFValue base;
} pyRDFValue; 

staticforward PyTypeObject RDFValue_Type;
static int pyRDFValue_init(pyRDFValue *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFValue_parse(pyRDFValue *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFValue_encode(pyRDFValue *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFValue_decode(pyRDFValue *self, PyObject *args, PyObject *kwds);
static PyObject *pyRDFValue_serialise(pyRDFValue *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  Resolver base;
} pyResolver; 

staticforward PyTypeObject Resolver_Type;
static int pyResolver_init(pyResolver *self, PyObject *args, PyObject *kwds);
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
static PyObject *pyResolver_parse(pyResolver *self, PyObject *args, PyObject *kwds);
static PyObject *pyResolver_lock(pyResolver *self, PyObject *args, PyObject *kwds);
static PyObject *pyResolver_unlock(pyResolver *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  FileBackedObject base;
} pyFileBackedObject; 

staticforward PyTypeObject FileBackedObject_Type;
static int pyFileBackedObject_init(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_set_property(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_finish(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_seek(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_read(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_write(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_tell(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_get_data(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_truncate(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_close(pyFileBackedObject *self, PyObject *args, PyObject *kwds);
static PyObject *pyFileBackedObject_Con(pyFileBackedObject *self, PyObject *args, PyObject *kwds);

typedef struct {
  PyObject_HEAD
  FileLikeObject base;
} pyFileLikeObject; 

staticforward PyTypeObject FileLikeObject_Type;
static int pyFileLikeObject_init(pyFileLikeObject *self, PyObject *args, PyObject *kwds);
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

typedef struct {
  PyObject_HEAD
  XSDInteger base;
} pyXSDInteger; 

staticforward PyTypeObject XSDInteger_Type;
static int pyXSDInteger_init(pyXSDInteger *self, PyObject *args, PyObject *kwds);
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
    return -1;
  };  return 0;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDString.parse(char * serialised_form;
);
********************************************************/
static PyObject *pyXSDString_parse(pyXSDString *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * serialised_form;
static char *kwlist[] = {"serialised_form", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &serialised_form))
 return NULL;

func_return = ((RDFValue)self->base)->parse(((RDFValue)self->base), serialised_form);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA * XSDString.encode();
********************************************************/
static PyObject *pyXSDString_encode(pyXSDString *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
TDB_DATA * func_return;
func_return = ((RDFValue)self->base)->encode(((RDFValue)self->base));
py_result = PyString_FromStringAndSize((char *)func_return->dptr, func_return->dsize);
talloc_free(func_return);returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDString.decode(char * data; int length;
);
********************************************************/
static PyObject *pyXSDString_decode(pyXSDString *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * data; int length;
static char *kwlist[] = {"data", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#", kwlist, data, &length))
 return NULL;

func_return = ((RDFValue)self->base)->decode(((RDFValue)self->base), data, length);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
char * XSDString.serialise();
********************************************************/
static PyObject *pyXSDString_serialise(pyXSDString *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * func_return;
func_return = ((RDFValue)self->base)->serialise(((RDFValue)self->base));
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
RDFValue XSDString.set(char * string; int length;
);
********************************************************/
static PyObject *pyXSDString_set(pyXSDString *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyRDFValue *func_return;
char * string; int length;
static char *kwlist[] = {"string", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#", kwlist, string, &length))
 return NULL;

{
       Object returned_object = (Object)((XSDString)self->base)->set(((XSDString)self->base), string, length);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFValue");
         return NULL;
       };

       func_return = (pyRDFValue *)new_class_wrapper(returned_object);
       func_return->base = (RDFValue)returned_object;
}
talloc_increase_ref_count(func_return->base);
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
char * XSDString.get();
********************************************************/
static PyObject *pyXSDString_get(pyXSDString *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * func_return;
func_return = ((XSDString)self->base)->get(((XSDString)self->base));
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;
};

static PyMethodDef XSDString_methods[] = {
     {"parse",(PyCFunction)pyXSDString_parse, METH_VARARGS|METH_KEYWORDS, ""},
     {"encode",(PyCFunction)pyXSDString_encode, METH_VARARGS|METH_KEYWORDS, ""},
     {"decode",(PyCFunction)pyXSDString_decode, METH_VARARGS|METH_KEYWORDS, ""},
     {"serialise",(PyCFunction)pyXSDString_serialise, METH_VARARGS|METH_KEYWORDS, ""},
     {"set",(PyCFunction)pyXSDString_set, METH_VARARGS|METH_KEYWORDS, ""},
     {"get",(PyCFunction)pyXSDString_get, METH_VARARGS|METH_KEYWORDS, ""},
     {NULL}  /* Sentinel */
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
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "XSDString",     /* tp_doc */
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
static void
AFFObject_dealloc(pyAFFObject *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyAFFObject_init(pyAFFObject *self, PyObject *args, PyObject *kwds) {
pyRDFURN *urn;
char mode; char *str_mode;
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os", kwlist, &urn,&str_mode))
 return -1;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  return -1;
};

mode = str_mode[0];

self->base = CONSTRUCT(AFFObject, AFFObject, Con, NULL, urn->base, mode);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class AFFObject");
    return -1;
  };  return 0;
};


/********************************************************
Autogenerated wrapper for function:
void * AFFObject.set_property(char * attribute;
pyRDFValue *value;
);
********************************************************/
static PyObject *pyAFFObject_set_property(pyAFFObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * attribute;
pyRDFValue *value;
static char *kwlist[] = {"attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "sO", kwlist, &attribute,&value))
 return NULL;

((AFFObject)self->base)->set_property(((AFFObject)self->base), attribute, value->base);
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
AFFObject AFFObject.finish();
********************************************************/
static PyObject *pyAFFObject_finish(pyAFFObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyAFFObject *func_return;
{
       Object returned_object = (Object)((AFFObject)self->base)->finish(((AFFObject)self->base));

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject");
         return NULL;
       };

       func_return = (pyAFFObject *)new_class_wrapper(returned_object);
       func_return->base = (AFFObject)returned_object;
}
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};

static PyMethodDef AFFObject_methods[] = {
     {"set_property",(PyCFunction)pyAFFObject_set_property, METH_VARARGS|METH_KEYWORDS, ""},
     {"finish",(PyCFunction)pyAFFObject_finish, METH_VARARGS|METH_KEYWORDS, ""},
     {NULL}  /* Sentinel */
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
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "AFFObject",     /* tp_doc */
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
    return -1;
  };  return 0;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDDatetime.parse(char * serialised_form;
);
********************************************************/
static PyObject *pyXSDDatetime_parse(pyXSDDatetime *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * serialised_form;
static char *kwlist[] = {"serialised_form", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &serialised_form))
 return NULL;

func_return = ((RDFValue)self->base)->parse(((RDFValue)self->base), serialised_form);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA * XSDDatetime.encode();
********************************************************/
static PyObject *pyXSDDatetime_encode(pyXSDDatetime *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
TDB_DATA * func_return;
func_return = ((RDFValue)self->base)->encode(((RDFValue)self->base));
py_result = PyString_FromStringAndSize((char *)func_return->dptr, func_return->dsize);
talloc_free(func_return);returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDDatetime.decode(char * data; int length;
);
********************************************************/
static PyObject *pyXSDDatetime_decode(pyXSDDatetime *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * data; int length;
static char *kwlist[] = {"data", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#", kwlist, data, &length))
 return NULL;

func_return = ((RDFValue)self->base)->decode(((RDFValue)self->base), data, length);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
char * XSDDatetime.serialise();
********************************************************/
static PyObject *pyXSDDatetime_serialise(pyXSDDatetime *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * func_return;
func_return = ((RDFValue)self->base)->serialise(((RDFValue)self->base));
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
RDFValue XSDDatetime.set(float time_flt; struct timeval time;
);
********************************************************/
static PyObject *pyXSDDatetime_set(pyXSDDatetime *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyRDFValue *func_return;
float time_flt; struct timeval time;
static char *kwlist[] = {"time", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "f", kwlist, &time_flt))
 return NULL;

time.tv_sec = (int)time_flt; time.tv_usec = (time_flt - time.tv_sec) * 1e6;
{
       Object returned_object = (Object)((XSDDatetime)self->base)->set(((XSDDatetime)self->base), time);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFValue");
         return NULL;
       };

       func_return = (pyRDFValue *)new_class_wrapper(returned_object);
       func_return->base = (RDFValue)returned_object;
}
talloc_increase_ref_count(func_return->base);
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};

static PyMethodDef XSDDatetime_methods[] = {
     {"parse",(PyCFunction)pyXSDDatetime_parse, METH_VARARGS|METH_KEYWORDS, ""},
     {"encode",(PyCFunction)pyXSDDatetime_encode, METH_VARARGS|METH_KEYWORDS, ""},
     {"decode",(PyCFunction)pyXSDDatetime_decode, METH_VARARGS|METH_KEYWORDS, ""},
     {"serialise",(PyCFunction)pyXSDDatetime_serialise, METH_VARARGS|METH_KEYWORDS, ""},
     {"set",(PyCFunction)pyXSDDatetime_set, METH_VARARGS|METH_KEYWORDS, ""},
     {NULL}  /* Sentinel */
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
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "XSDDatetime",     /* tp_doc */
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

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &url))
 return -1;


self->base = CONSTRUCT(URLParse, URLParse, Con, NULL, url);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class URLParse");
    return -1;
  };  return 0;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  URLParse.parse(char * url;
);
********************************************************/
static PyObject *pyURLParse_parse(pyURLParse *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * url;
static char *kwlist[] = {"url", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &url))
 return NULL;

func_return = ((URLParse)self->base)->parse(((URLParse)self->base), url);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
char * URLParse.string();
********************************************************/
static PyObject *pyURLParse_string(pyURLParse *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * func_return;
func_return = ((URLParse)self->base)->string(((URLParse)self->base), NULL);
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;
};

static PyMethodDef URLParse_methods[] = {
     {"parse",(PyCFunction)pyURLParse_parse, METH_VARARGS|METH_KEYWORDS, ""},
     {"string",(PyCFunction)pyURLParse_string, METH_VARARGS|METH_KEYWORDS, ""},
     {NULL}  /* Sentinel */
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
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "URLParse",     /* tp_doc */
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
static void
TDB_DATA_LIST_dealloc(pyTDB_DATA_LIST *self) {
if(self->base) talloc_free(self->base);
};

static int pyTDB_DATA_LIST_init(pyTDB_DATA_LIST *self, PyObject *args, PyObject *kwds) {

self->base = NULL;
  return 0;
};

static PyMethodDef TDB_DATA_LIST_methods[] = {
     {NULL}  /* Sentinel */
};

static PyTypeObject TDB_DATA_LIST_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "pyaff4.TDB_DATA_LIST",               /* tp_name */
    sizeof(pyTDB_DATA_LIST),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)TDB_DATA_LIST_dealloc,/* tp_dealloc */
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
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "TDB_DATA_LIST",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    TDB_DATA_LIST_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pyTDB_DATA_LIST_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
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
    return -1;
  };  return 0;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  RDFValue.parse(char * serialised_form;
);
********************************************************/
static PyObject *pyRDFValue_parse(pyRDFValue *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * serialised_form;
static char *kwlist[] = {"serialised_form", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &serialised_form))
 return NULL;

func_return = ((RDFValue)self->base)->parse(((RDFValue)self->base), serialised_form);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA * RDFValue.encode();
********************************************************/
static PyObject *pyRDFValue_encode(pyRDFValue *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
TDB_DATA * func_return;
func_return = ((RDFValue)self->base)->encode(((RDFValue)self->base));
py_result = PyString_FromStringAndSize((char *)func_return->dptr, func_return->dsize);
talloc_free(func_return);returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  RDFValue.decode(char * data; int length;
);
********************************************************/
static PyObject *pyRDFValue_decode(pyRDFValue *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * data; int length;
static char *kwlist[] = {"data", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#", kwlist, data, &length))
 return NULL;

func_return = ((RDFValue)self->base)->decode(((RDFValue)self->base), data, length);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
char * RDFValue.serialise();
********************************************************/
static PyObject *pyRDFValue_serialise(pyRDFValue *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * func_return;
func_return = ((RDFValue)self->base)->serialise(((RDFValue)self->base));
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;
};

static PyMethodDef RDFValue_methods[] = {
     {"parse",(PyCFunction)pyRDFValue_parse, METH_VARARGS|METH_KEYWORDS, ""},
     {"encode",(PyCFunction)pyRDFValue_encode, METH_VARARGS|METH_KEYWORDS, ""},
     {"decode",(PyCFunction)pyRDFValue_decode, METH_VARARGS|METH_KEYWORDS, ""},
     {"serialise",(PyCFunction)pyRDFValue_serialise, METH_VARARGS|METH_KEYWORDS, ""},
     {NULL}  /* Sentinel */
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
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "RDFValue",     /* tp_doc */
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
    return -1;
  };  return 0;
};


/********************************************************
Autogenerated wrapper for function:
AFFObject Resolver.open(pyRDFURN *uri;
char mode; char *str_mode;
);
********************************************************/
static PyObject *pyResolver_open(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyAFFObject *func_return;
pyRDFURN *uri;
char mode; char *str_mode;
static char *kwlist[] = {"uri","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os", kwlist, &uri,&str_mode))
 return NULL;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  return NULL;
};

mode = str_mode[0];
{
       Object returned_object = (Object)((Resolver)self->base)->open(((Resolver)self->base), uri->base, mode);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject");
         return NULL;
       };

       func_return = (pyAFFObject *)new_class_wrapper(returned_object);
       func_return->base = (AFFObject)returned_object;
}
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
void * Resolver.cache_return(pyAFFObject *obj;
);
********************************************************/
static PyObject *pyResolver_cache_return(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyAFFObject *obj;
static char *kwlist[] = {"obj", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &obj))
 return NULL;

((Resolver)self->base)->cache_return(((Resolver)self->base), obj->base);
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
AFFObject Resolver.create(char * name; char mode;
);
********************************************************/
static PyObject *pyResolver_create(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyAFFObject *func_return;
char * name; char mode;
static char *kwlist[] = {"name", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#", kwlist, name, &mode))
 return NULL;

{
       Object returned_object = (Object)((Resolver)self->base)->create(((Resolver)self->base), name, mode);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject");
         return NULL;
       };

       func_return = (pyAFFObject *)new_class_wrapper(returned_object);
       func_return->base = (AFFObject)returned_object;
}
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Resolver.resolve_value(pyRDFURN *uri;
char * attribute;
pyRDFValue *value;
);
********************************************************/
static PyObject *pyResolver_resolve_value(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
pyRDFURN *uri;
char * attribute;
pyRDFValue *value;
static char *kwlist[] = {"uri","attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "OsO", kwlist, &uri,&attribute,&value))
 return NULL;

func_return = ((Resolver)self->base)->resolve_value(((Resolver)self->base), uri->base, attribute, value->base);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
RESOLVER_ITER Resolver.get_iter(pyRDFURN *uri;
char * attribute;
);
********************************************************/
static PyObject *pyResolver_get_iter(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyRESOLVER_ITER *func_return;
pyRDFURN *uri;
char * attribute;
static char *kwlist[] = {"uri","attribute", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os", kwlist, &uri,&attribute))
 return NULL;


func_return = (pyRESOLVER_ITER *)PyObject_New(pyRESOLVER_ITER, &RESOLVER_ITER_Type);
func_return->base = ((Resolver)self->base)->get_iter(((Resolver)self->base), NULL, uri->base, attribute);
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Resolver.iter_next(pyRESOLVER_ITER *iter;
pyRDFValue *result;
);
********************************************************/
static PyObject *pyResolver_iter_next(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
pyRESOLVER_ITER *iter;
pyRDFValue *result;
static char *kwlist[] = {"iter","result", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "OO", kwlist, &iter,&result))
 return NULL;

func_return = ((Resolver)self->base)->iter_next(((Resolver)self->base), iter->base, result->base);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
RDFValue Resolver.iter_next_alloc(pyRESOLVER_ITER *iter;
);
********************************************************/
static PyObject *pyResolver_iter_next_alloc(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyRDFValue *func_return;
pyRESOLVER_ITER *iter;
static char *kwlist[] = {"iter", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &iter))
 return NULL;

{
       Object returned_object = (Object)((Resolver)self->base)->iter_next_alloc(((Resolver)self->base), iter->base);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFValue");
         return NULL;
       };

       func_return = (pyRDFValue *)new_class_wrapper(returned_object);
       func_return->base = (RDFValue)returned_object;
}
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
void * Resolver.del(pyRDFURN *uri;
char * attribute;
);
********************************************************/
static PyObject *pyResolver_del(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyRDFURN *uri;
char * attribute;
static char *kwlist[] = {"uri","attribute", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os", kwlist, &uri,&attribute))
 return NULL;

((Resolver)self->base)->del(((Resolver)self->base), uri->base, attribute);
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
void * Resolver.set_value(pyRDFURN *uri;
char * attribute;
pyRDFValue *value;
);
********************************************************/
static PyObject *pyResolver_set_value(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyRDFURN *uri;
char * attribute;
pyRDFValue *value;
static char *kwlist[] = {"uri","attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "OsO", kwlist, &uri,&attribute,&value))
 return NULL;

((Resolver)self->base)->set_value(((Resolver)self->base), uri->base, attribute, value->base);
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
void * Resolver.add_value(pyRDFURN *uri;
char * attribute;
pyRDFValue *value;
);
********************************************************/
static PyObject *pyResolver_add_value(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyRDFURN *uri;
char * attribute;
pyRDFValue *value;
static char *kwlist[] = {"uri","attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "OsO", kwlist, &uri,&attribute,&value))
 return NULL;

((Resolver)self->base)->add_value(((Resolver)self->base), uri->base, attribute, value->base);
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
void * Resolver.parse(char * context;
char * text; int len;
);
********************************************************/
static PyObject *pyResolver_parse(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * context;
char * text; int len;
static char *kwlist[] = {"context","text", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "ss#", kwlist, &context,text, &len))
 return NULL;

((Resolver)self->base)->parse(((Resolver)self->base), context, text, len);
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Resolver.lock(pyRDFURN *urn;
char mode; char *str_mode;
);
********************************************************/
static PyObject *pyResolver_lock(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
pyRDFURN *urn;
char mode; char *str_mode;
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os", kwlist, &urn,&str_mode))
 return NULL;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  return NULL;
};

mode = str_mode[0];
func_return = ((Resolver)self->base)->lock(((Resolver)self->base), urn->base, mode);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  Resolver.unlock(pyRDFURN *urn;
char mode; char *str_mode;
);
********************************************************/
static PyObject *pyResolver_unlock(pyResolver *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
pyRDFURN *urn;
char mode; char *str_mode;
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os", kwlist, &urn,&str_mode))
 return NULL;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  return NULL;
};

mode = str_mode[0];
func_return = ((Resolver)self->base)->unlock(((Resolver)self->base), urn->base, mode);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};

static PyMethodDef Resolver_methods[] = {
     {"open",(PyCFunction)pyResolver_open, METH_VARARGS|METH_KEYWORDS, ""},
     {"cache_return",(PyCFunction)pyResolver_cache_return, METH_VARARGS|METH_KEYWORDS, ""},
     {"create",(PyCFunction)pyResolver_create, METH_VARARGS|METH_KEYWORDS, ""},
     {"resolve_value",(PyCFunction)pyResolver_resolve_value, METH_VARARGS|METH_KEYWORDS, ""},
     {"get_iter",(PyCFunction)pyResolver_get_iter, METH_VARARGS|METH_KEYWORDS, ""},
     {"iter_next",(PyCFunction)pyResolver_iter_next, METH_VARARGS|METH_KEYWORDS, ""},
     {"iter_next_alloc",(PyCFunction)pyResolver_iter_next_alloc, METH_VARARGS|METH_KEYWORDS, ""},
     {"del",(PyCFunction)pyResolver_del, METH_VARARGS|METH_KEYWORDS, ""},
     {"set_value",(PyCFunction)pyResolver_set_value, METH_VARARGS|METH_KEYWORDS, ""},
     {"add_value",(PyCFunction)pyResolver_add_value, METH_VARARGS|METH_KEYWORDS, ""},
     {"parse",(PyCFunction)pyResolver_parse, METH_VARARGS|METH_KEYWORDS, ""},
     {"lock",(PyCFunction)pyResolver_lock, METH_VARARGS|METH_KEYWORDS, ""},
     {"unlock",(PyCFunction)pyResolver_unlock, METH_VARARGS|METH_KEYWORDS, ""},
     {NULL}  /* Sentinel */
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
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "Resolver",     /* tp_doc */
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
static void
FileBackedObject_dealloc(pyFileBackedObject *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyFileBackedObject_init(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
pyRDFURN *urn;
char mode; char *str_mode;
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os", kwlist, &urn,&str_mode))
 return -1;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  return -1;
};

mode = str_mode[0];

self->base = CONSTRUCT(FileBackedObject, AFFObject, Con, NULL, urn->base, mode);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class FileBackedObject");
    return -1;
  };  return 0;
};


/********************************************************
Autogenerated wrapper for function:
void * FileBackedObject.set_property(char * attribute;
pyRDFValue *value;
);
********************************************************/
static PyObject *pyFileBackedObject_set_property(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * attribute;
pyRDFValue *value;
static char *kwlist[] = {"attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "sO", kwlist, &attribute,&value))
 return NULL;

((AFFObject)self->base)->set_property(((AFFObject)self->base), attribute, value->base);
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
AFFObject FileBackedObject.finish();
********************************************************/
static PyObject *pyFileBackedObject_finish(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyAFFObject *func_return;
{
       Object returned_object = (Object)((AFFObject)self->base)->finish(((AFFObject)self->base));

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject");
         return NULL;
       };

       func_return = (pyAFFObject *)new_class_wrapper(returned_object);
       func_return->base = (AFFObject)returned_object;
}
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileBackedObject.seek(uint64_t  offset;
uint64_t  whence;
);
********************************************************/
static PyObject *pyFileBackedObject_seek(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
uint64_t  whence;
static char *kwlist[] = {"offset","whence", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "KK", kwlist, &offset,&whence))
 return NULL;

func_return = ((FileLikeObject)self->base)->seek(((FileLikeObject)self->base), offset, whence);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileBackedObject.read(OUT char * buffer; unsigned long int length;
PyObject *tmp_buffer;
);
********************************************************/
static PyObject *pyFileBackedObject_read(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
OUT char * buffer; unsigned long int length;
PyObject *tmp_buffer;
static char *kwlist[] = {"length", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "l", kwlist, &length))
 return NULL;

tmp_buffer = PyString_FromStringAndSize(NULL, length);
PyString_AsStringAndSize(tmp_buffer, &buffer, &length);
func_return = ((FileLikeObject)self->base)->read(((FileLikeObject)self->base), buffer, length);
returned_result = PyList_New(0);
py_result = PyLong_FromLong(func_return);
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
 _PyString_Resize(&tmp_buffer, func_return); 
py_result = tmp_buffer;
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileBackedObject.write(char * buffer; unsigned long int length;
);
********************************************************/
static PyObject *pyFileBackedObject_write(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * buffer; unsigned long int length;
static char *kwlist[] = {"buffer", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#", kwlist, buffer, &length))
 return NULL;

func_return = ((FileLikeObject)self->base)->write(((FileLikeObject)self->base), buffer, length);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileBackedObject.tell();
********************************************************/
static PyObject *pyFileBackedObject_tell(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
func_return = ((FileLikeObject)self->base)->tell(((FileLikeObject)self->base));
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
func_return = ((FileLikeObject)self->base)->get_data(((FileLikeObject)self->base));
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileBackedObject.truncate(uint64_t  offset;
);
********************************************************/
static PyObject *pyFileBackedObject_truncate(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
static char *kwlist[] = {"offset", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "K", kwlist, &offset))
 return NULL;

func_return = ((FileLikeObject)self->base)->truncate(((FileLikeObject)self->base), offset);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
void * FileBackedObject.close();
********************************************************/
static PyObject *pyFileBackedObject_close(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
((FileLikeObject)self->base)->close(((FileLikeObject)self->base));
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
FileBackedObject FileBackedObject.Con(pyRDFURN *fileurn;
char mode; char *str_mode;
);
********************************************************/
static PyObject *pyFileBackedObject_Con(pyFileBackedObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyFileBackedObject *func_return;
pyRDFURN *fileurn;
char mode; char *str_mode;
static char *kwlist[] = {"fileurn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os", kwlist, &fileurn,&str_mode))
 return NULL;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  return NULL;
};

mode = str_mode[0];
{
       Object returned_object = (Object)((FileBackedObject)self->base)->Con(((FileBackedObject)self->base), fileurn->base, mode);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object FileBackedObject");
         return NULL;
       };

       func_return = (pyFileBackedObject *)new_class_wrapper(returned_object);
       func_return->base = (FileBackedObject)returned_object;
}
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};

static PyMethodDef FileBackedObject_methods[] = {
     {"set_property",(PyCFunction)pyFileBackedObject_set_property, METH_VARARGS|METH_KEYWORDS, ""},
     {"finish",(PyCFunction)pyFileBackedObject_finish, METH_VARARGS|METH_KEYWORDS, ""},
     {"seek",(PyCFunction)pyFileBackedObject_seek, METH_VARARGS|METH_KEYWORDS, ""},
     {"read",(PyCFunction)pyFileBackedObject_read, METH_VARARGS|METH_KEYWORDS, ""},
     {"write",(PyCFunction)pyFileBackedObject_write, METH_VARARGS|METH_KEYWORDS, ""},
     {"tell",(PyCFunction)pyFileBackedObject_tell, METH_VARARGS|METH_KEYWORDS, ""},
     {"get_data",(PyCFunction)pyFileBackedObject_get_data, METH_VARARGS|METH_KEYWORDS, ""},
     {"truncate",(PyCFunction)pyFileBackedObject_truncate, METH_VARARGS|METH_KEYWORDS, ""},
     {"close",(PyCFunction)pyFileBackedObject_close, METH_VARARGS|METH_KEYWORDS, ""},
     {"Con",(PyCFunction)pyFileBackedObject_Con, METH_VARARGS|METH_KEYWORDS, ""},
     {NULL}  /* Sentinel */
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
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "FileBackedObject",     /* tp_doc */
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
static void
FileLikeObject_dealloc(pyFileLikeObject *self) {

    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };

};

static int pyFileLikeObject_init(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
pyRDFURN *urn;
char mode; char *str_mode;
static char *kwlist[] = {"urn","mode", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os", kwlist, &urn,&str_mode))
 return -1;


if(strlen(str_mode)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg 'mode'");
  return -1;
};

mode = str_mode[0];

self->base = CONSTRUCT(FileLikeObject, AFFObject, Con, NULL, urn->base, mode);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class FileLikeObject");
    return -1;
  };  return 0;
};


/********************************************************
Autogenerated wrapper for function:
void * FileLikeObject.set_property(char * attribute;
pyRDFValue *value;
);
********************************************************/
static PyObject *pyFileLikeObject_set_property(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * attribute;
pyRDFValue *value;
static char *kwlist[] = {"attribute","value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "sO", kwlist, &attribute,&value))
 return NULL;

((AFFObject)self->base)->set_property(((AFFObject)self->base), attribute, value->base);
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
AFFObject FileLikeObject.finish();
********************************************************/
static PyObject *pyFileLikeObject_finish(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyAFFObject *func_return;
{
       Object returned_object = (Object)((AFFObject)self->base)->finish(((AFFObject)self->base));

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object AFFObject");
         return NULL;
       };

       func_return = (pyAFFObject *)new_class_wrapper(returned_object);
       func_return->base = (AFFObject)returned_object;
}
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileLikeObject.seek(uint64_t  offset;
uint64_t  whence;
);
********************************************************/
static PyObject *pyFileLikeObject_seek(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
uint64_t  whence;
static char *kwlist[] = {"offset","whence", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "KK", kwlist, &offset,&whence))
 return NULL;

func_return = ((FileLikeObject)self->base)->seek(((FileLikeObject)self->base), offset, whence);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileLikeObject.read(OUT char * buffer; unsigned long int length;
PyObject *tmp_buffer;
);
********************************************************/
static PyObject *pyFileLikeObject_read(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
OUT char * buffer; unsigned long int length;
PyObject *tmp_buffer;
static char *kwlist[] = {"length", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "l", kwlist, &length))
 return NULL;

tmp_buffer = PyString_FromStringAndSize(NULL, length);
PyString_AsStringAndSize(tmp_buffer, &buffer, &length);
func_return = ((FileLikeObject)self->base)->read(((FileLikeObject)self->base), buffer, length);
returned_result = PyList_New(0);
py_result = PyLong_FromLong(func_return);
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
 _PyString_Resize(&tmp_buffer, func_return); 
py_result = tmp_buffer;
PyList_Append(returned_result, py_result); Py_DECREF(py_result);
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileLikeObject.write(char * buffer; unsigned long int length;
);
********************************************************/
static PyObject *pyFileLikeObject_write(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * buffer; unsigned long int length;
static char *kwlist[] = {"buffer", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#", kwlist, buffer, &length))
 return NULL;

func_return = ((FileLikeObject)self->base)->write(((FileLikeObject)self->base), buffer, length);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileLikeObject.tell();
********************************************************/
static PyObject *pyFileLikeObject_tell(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
func_return = ((FileLikeObject)self->base)->tell(((FileLikeObject)self->base));
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
func_return = ((FileLikeObject)self->base)->get_data(((FileLikeObject)self->base));
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  FileLikeObject.truncate(uint64_t  offset;
);
********************************************************/
static PyObject *pyFileLikeObject_truncate(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
uint64_t  offset;
static char *kwlist[] = {"offset", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "K", kwlist, &offset))
 return NULL;

func_return = ((FileLikeObject)self->base)->truncate(((FileLikeObject)self->base), offset);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
void * FileLikeObject.close();
********************************************************/
static PyObject *pyFileLikeObject_close(pyFileLikeObject *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
((FileLikeObject)self->base)->close(((FileLikeObject)self->base));
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;
};

static PyMethodDef FileLikeObject_methods[] = {
     {"set_property",(PyCFunction)pyFileLikeObject_set_property, METH_VARARGS|METH_KEYWORDS, ""},
     {"finish",(PyCFunction)pyFileLikeObject_finish, METH_VARARGS|METH_KEYWORDS, ""},
     {"seek",(PyCFunction)pyFileLikeObject_seek, METH_VARARGS|METH_KEYWORDS, ""},
     {"read",(PyCFunction)pyFileLikeObject_read, METH_VARARGS|METH_KEYWORDS, ""},
     {"write",(PyCFunction)pyFileLikeObject_write, METH_VARARGS|METH_KEYWORDS, ""},
     {"tell",(PyCFunction)pyFileLikeObject_tell, METH_VARARGS|METH_KEYWORDS, ""},
     {"get_data",(PyCFunction)pyFileLikeObject_get_data, METH_VARARGS|METH_KEYWORDS, ""},
     {"truncate",(PyCFunction)pyFileLikeObject_truncate, METH_VARARGS|METH_KEYWORDS, ""},
     {"close",(PyCFunction)pyFileLikeObject_close, METH_VARARGS|METH_KEYWORDS, ""},
     {NULL}  /* Sentinel */
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
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "FileLikeObject",     /* tp_doc */
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
static void
RESOLVER_ITER_dealloc(pyRESOLVER_ITER *self) {
if(self->base) talloc_free(self->base);
};

static int pyRESOLVER_ITER_init(pyRESOLVER_ITER *self, PyObject *args, PyObject *kwds) {

self->base = NULL;
  return 0;
};

static PyMethodDef RESOLVER_ITER_methods[] = {
     {NULL}  /* Sentinel */
};

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
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "RESOLVER_ITER",     /* tp_doc */
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
    return -1;
  };  return 0;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDInteger.parse(char * serialised_form;
);
********************************************************/
static PyObject *pyXSDInteger_parse(pyXSDInteger *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * serialised_form;
static char *kwlist[] = {"serialised_form", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &serialised_form))
 return NULL;

func_return = ((RDFValue)self->base)->parse(((RDFValue)self->base), serialised_form);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA * XSDInteger.encode();
********************************************************/
static PyObject *pyXSDInteger_encode(pyXSDInteger *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
TDB_DATA * func_return;
func_return = ((RDFValue)self->base)->encode(((RDFValue)self->base));
py_result = PyString_FromStringAndSize((char *)func_return->dptr, func_return->dsize);
talloc_free(func_return);returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDInteger.decode(char * data; int length;
);
********************************************************/
static PyObject *pyXSDInteger_decode(pyXSDInteger *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * data; int length;
static char *kwlist[] = {"data", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#", kwlist, data, &length))
 return NULL;

func_return = ((RDFValue)self->base)->decode(((RDFValue)self->base), data, length);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
char * XSDInteger.serialise();
********************************************************/
static PyObject *pyXSDInteger_serialise(pyXSDInteger *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * func_return;
func_return = ((RDFValue)self->base)->serialise(((RDFValue)self->base));
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
RDFValue XSDInteger.set(uint64_t  value;
);
********************************************************/
static PyObject *pyXSDInteger_set(pyXSDInteger *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyRDFValue *func_return;
uint64_t  value;
static char *kwlist[] = {"value", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "K", kwlist, &value))
 return NULL;

{
       Object returned_object = (Object)((XSDInteger)self->base)->set(((XSDInteger)self->base), value);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFValue");
         return NULL;
       };

       func_return = (pyRDFValue *)new_class_wrapper(returned_object);
       func_return->base = (RDFValue)returned_object;
}
talloc_increase_ref_count(func_return->base);
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  XSDInteger.get();
********************************************************/
static PyObject *pyXSDInteger_get(pyXSDInteger *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
func_return = ((XSDInteger)self->base)->get(((XSDInteger)self->base));
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};

static PyMethodDef XSDInteger_methods[] = {
     {"parse",(PyCFunction)pyXSDInteger_parse, METH_VARARGS|METH_KEYWORDS, ""},
     {"encode",(PyCFunction)pyXSDInteger_encode, METH_VARARGS|METH_KEYWORDS, ""},
     {"decode",(PyCFunction)pyXSDInteger_decode, METH_VARARGS|METH_KEYWORDS, ""},
     {"serialise",(PyCFunction)pyXSDInteger_serialise, METH_VARARGS|METH_KEYWORDS, ""},
     {"set",(PyCFunction)pyXSDInteger_set, METH_VARARGS|METH_KEYWORDS, ""},
     {"get",(PyCFunction)pyXSDInteger_get, METH_VARARGS|METH_KEYWORDS, ""},
     {NULL}  /* Sentinel */
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
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "XSDInteger",     /* tp_doc */
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
    return -1;
  };  return 0;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  RDFURN.parse(char * serialised_form;
);
********************************************************/
static PyObject *pyRDFURN_parse(pyRDFURN *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * serialised_form;
static char *kwlist[] = {"serialised_form", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &serialised_form))
 return NULL;

func_return = ((RDFValue)self->base)->parse(((RDFValue)self->base), serialised_form);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA * RDFURN.encode();
********************************************************/
static PyObject *pyRDFURN_encode(pyRDFURN *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
TDB_DATA * func_return;
func_return = ((RDFValue)self->base)->encode(((RDFValue)self->base));
py_result = PyString_FromStringAndSize((char *)func_return->dptr, func_return->dsize);
talloc_free(func_return);returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
uint64_t  RDFURN.decode(char * data; int length;
);
********************************************************/
static PyObject *pyRDFURN_decode(pyRDFURN *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
uint64_t  func_return;
char * data; int length;
static char *kwlist[] = {"data", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s#", kwlist, data, &length))
 return NULL;

func_return = ((RDFValue)self->base)->decode(((RDFValue)self->base), data, length);
py_result = PyLong_FromLong(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
char * RDFURN.serialise();
********************************************************/
static PyObject *pyRDFURN_serialise(pyRDFURN *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * func_return;
func_return = ((RDFValue)self->base)->serialise(((RDFValue)self->base));
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
RDFValue RDFURN.set(char * urn;
);
********************************************************/
static PyObject *pyRDFURN_set(pyRDFURN *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyRDFValue *func_return;
char * urn;
static char *kwlist[] = {"urn", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &urn))
 return NULL;

{
       Object returned_object = (Object)((RDFURN)self->base)->set(((RDFURN)self->base), urn);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFValue");
         return NULL;
       };

       func_return = (pyRDFValue *)new_class_wrapper(returned_object);
       func_return->base = (RDFValue)returned_object;
}
talloc_increase_ref_count(func_return->base);
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
char * RDFURN.get();
********************************************************/
static PyObject *pyRDFURN_get(pyRDFURN *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * func_return;
func_return = ((RDFURN)self->base)->get(((RDFURN)self->base));
py_result = PyString_FromStringAndSize((char *)func_return, strlen(func_return));
talloc_free(func_return);
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
RDFURN RDFURN.copy();
********************************************************/
static PyObject *pyRDFURN_copy(pyRDFURN *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
pyRDFURN *func_return;
{
       Object returned_object = (Object)((RDFURN)self->base)->copy(((RDFURN)self->base), NULL);

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object RDFURN");
         return NULL;
       };

       func_return = (pyRDFURN *)new_class_wrapper(returned_object);
       func_return->base = (RDFURN)returned_object;
}
py_result = (PyObject *)func_return;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
void * RDFURN.add(char * urn;
);
********************************************************/
static PyObject *pyRDFURN_add(pyRDFURN *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
char * urn;
static char *kwlist[] = {"urn", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &urn))
 return NULL;

((RDFURN)self->base)->add(((RDFURN)self->base), urn);
Py_INCREF(Py_None); py_result = Py_None;
returned_result = py_result;
return returned_result;
};


/********************************************************
Autogenerated wrapper for function:
TDB_DATA RDFURN.relative_name(pyRDFURN *volume;
);
********************************************************/
static PyObject *pyRDFURN_relative_name(pyRDFURN *self, PyObject *args, PyObject *kwds) {
PyObject *returned_result, *py_result;
TDB_DATA func_return;
pyRDFURN *volume;
static char *kwlist[] = {"volume", NULL};

if(!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &volume))
 return NULL;

func_return = ((RDFURN)self->base)->relative_name(((RDFURN)self->base), volume->base);
py_result = PyString_FromStringAndSize((char *)func_return.dptr, func_return.dsize);
returned_result = py_result;
return returned_result;
};

static PyMethodDef RDFURN_methods[] = {
     {"parse",(PyCFunction)pyRDFURN_parse, METH_VARARGS|METH_KEYWORDS, ""},
     {"encode",(PyCFunction)pyRDFURN_encode, METH_VARARGS|METH_KEYWORDS, ""},
     {"decode",(PyCFunction)pyRDFURN_decode, METH_VARARGS|METH_KEYWORDS, ""},
     {"serialise",(PyCFunction)pyRDFURN_serialise, METH_VARARGS|METH_KEYWORDS, ""},
     {"set",(PyCFunction)pyRDFURN_set, METH_VARARGS|METH_KEYWORDS, ""},
     {"get",(PyCFunction)pyRDFURN_get, METH_VARARGS|METH_KEYWORDS, ""},
     {"copy",(PyCFunction)pyRDFURN_copy, METH_VARARGS|METH_KEYWORDS, ""},
     {"add",(PyCFunction)pyRDFURN_add, METH_VARARGS|METH_KEYWORDS, ""},
     {"relative_name",(PyCFunction)pyRDFURN_relative_name, METH_VARARGS|METH_KEYWORDS, ""},
     {NULL}  /* Sentinel */
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
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    "RDFURN",     /* tp_doc */
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

 TDB_DATA_LIST_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&TDB_DATA_LIST_Type) < 0)
     return;

 Py_INCREF((PyObject *)&TDB_DATA_LIST_Type);
 PyModule_AddObject(m, "TDB_DATA_LIST", (PyObject *)&TDB_DATA_LIST_Type);

 RDFValue_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&RDFValue_Type) < 0)
     return;

 Py_INCREF((PyObject *)&RDFValue_Type);
 PyModule_AddObject(m, "RDFValue", (PyObject *)&RDFValue_Type);

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
 tmp = PyString_FromString(AFF4_VOLATILE_CONTAINS); 

 PyDict_SetItemString(d, "AFF4_VOLATILE_CONTAINS", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_IDENTITY_STORED); 

 PyDict_SetItemString(d, "AFF4_IDENTITY_STORED", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_AUTOLOAD); 

 PyDict_SetItemString(d, "AFF4_AUTOLOAD", tmp);
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
 tmp = PyString_FromString(AFF4_IDENTITY); 

 PyDict_SetItemString(d, "AFF4_IDENTITY", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_SPECIAL_URN_NULL); 

 PyDict_SetItemString(d, "AFF4_SPECIAL_URN_NULL", tmp);
 Py_DECREF(tmp);
 tmp = PyString_FromString(AFF4_SPECIAL_URN_ZERO); 

 PyDict_SetItemString(d, "AFF4_SPECIAL_URN_ZERO", tmp);
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

talloc_enable_leak_report_full();
AFF4_Init();

python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__XSDString;
python_wrappers[TOTAL_CLASSES++].python_type = &XSDString_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__AFFObject;
python_wrappers[TOTAL_CLASSES++].python_type = &AFFObject_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__XSDDatetime;
python_wrappers[TOTAL_CLASSES++].python_type = &XSDDatetime_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__URLParse;
python_wrappers[TOTAL_CLASSES++].python_type = &URLParse_Type;
python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__RDFValue;
python_wrappers[TOTAL_CLASSES++].python_type = &RDFValue_Type;
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

