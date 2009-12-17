import sys, os, re, pdb

def log(msg):
    sys.stderr.write(msg+"\n")

class Module:
    def __init__(self, name):
        self.name = name
        self.constants = []
        self.classes = {}
        self.headers = "#include <Python.h>\n"

    def initialization(self):
        result = """
talloc_enable_leak_report_full();
AFF4_Init();

"""
        for cls in self.classes.values():
            result += cls.initialise()

        return result

    def add_constant(self, constant, type="numeric"):
        """ This will be called to add #define constant macros """
        self.constants.append((constant, type))

    def add_class(self, cls, handler):
        self.classes[cls.class_name] = cls

        ## Make a wrapper in the type dispatcher so we can handle
        ## passing this class from/to python
        type_dispatcher[cls.class_name] = handler

    def private_functions(self):
        """ Emits hard coded private functions for doing various things """
        return """
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
} python_wrappers[%s];

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

""" % (len(self.classes)+1)

    def write(self, out):
        out.write(self.headers)
        out.write(self.private_functions())

        for cls in self.classes.values():
            cls.struct(out)
            cls.prototypes(out)

        out.write("/*****************************************************\n             Implementation\n******************************************************/\n\n")
        for cls in self.classes.values():
            cls.code(out)
            cls.PyMethodDef(out)
            cls.PyTypeObject(out)


        ## Write the module initializer
        out.write("""
static PyMethodDef %(module)s_methods[] = {
     {NULL}  /* Sentinel */
};

PyMODINIT_FUNC init%(module)s(void) {
   /* create module */
   PyObject *m = Py_InitModule3("%(module)s", %(module)s_methods,
                                   "%(module)s module.");
   PyObject *d = PyModule_GetDict(m);
   PyObject *tmp;
""" % {'module': self.name})

        for cls in self.classes.values():
            out.write("""
 %(name)s_Type.tp_new = PyType_GenericNew;
 if (PyType_Ready(&%(name)s_Type) < 0)
     return;

 Py_INCREF((PyObject *)&%(name)s_Type);
 PyModule_AddObject(m, "%(name)s", (PyObject *)&%(name)s_Type);
""" % {'name': cls.class_name})

        ## Add the constants in here
        for constant, type in self.constants:
            if type == 'numeric':
                out.write(""" tmp = PyLong_FromUnsignedLongLong(%s); \n""" % constant)
            elif type == 'string':
                out.write(" tmp = PyString_FromString(%s); \n" % constant)

            out.write("""
 PyDict_SetItemString(d, "%s", tmp);
 Py_DECREF(tmp);\n""" % (constant))

        out.write(self.initialization())
        out.write("}\n\n")

class Type:
    interface = None
    buidstr = 'O'
    sense = 'IN'

    def __init__(self, name, type):
        self.name = name
        self.type = type
        self.attributes = set()

    def python_name(self):
        return self.name

    def definition(self):
        return "%s %s;\n" % (self.type, self.name)

    def byref(self):
        return "&%s" % self.name

    def call_arg(self):
        return self.name

    def pre_call(self, method):
        return ''

    def assign(self, call, method):
        return "%s = %s;\n" % (self.name, call)

class String(Type):
    interface = 'string'
    buidstr = 's'

    def __init__(self, name, type):
        Type.__init__(self, name, type)
        self.length = "strlen(%s)" % name

    def byref(self):
        return "&%s" % self.name

    def to_python_object(self):
        return "py_result = PyString_FromStringAndSize((char *)%s, %s);\ntalloc_free(%s);\n" % (self.name, self.length, self.name)

class BorrowedString(String):
    def to_python_object(self):
        return "py_result = PyString_FromStringAndSize((char *)%(name)s, %(length)s);\n" % self.__dict__

class Char_and_Length(Type):
    interface = 'string'
    buidstr = 's#'

    def __init__(self, data, data_type, length, length_type):
        Type.__init__(self, data, None)

        self.name = data
        self.data_type=data_type
        self.length = length
        self.length_type = length_type

    def definition(self):
        return "%s %s; %s %s;\n" % (self.data_type, self.name,
                                    self.length_type, self.length)

    def byref(self):
        return "%s, &%s" % (self.name, self.length)

    def call_arg(self):
        return "%s, %s" % (self.name, self.length)

class Integer(Type):
    interface = 'integer'
    buidstr = 'K'

    def __init__(self, name,type):
        Type.__init__(self,name,type)
        self.type = 'uint64_t '

    def to_python_object(self):
        return "py_result = PyLong_FromLong(%s);\n" % self.name

class Char(Integer):
    buidstr = "s"

    def definition(self):
        return "char %s; char *str_%s;\n" % (self.name, self.name)

    def byref(self):
        return "&str_%s" % self.name

    def pre_call(self, method):
        return """
if(strlen(str_%(name)s)!=1) {
  PyErr_Format(PyExc_RuntimeError,
          "You must only provide a single character for arg %(name)r");
  %(error)s;
};

%(name)s = str_%(name)s[0];
""" % dict(name = self.name, error = method.error_condition())

class StringOut(String):
    sense = 'OUT'

class Char_and_Length_OUT(Char_and_Length):
    sense = 'OUT'
    buidstr = 'l'

    def definition(self):
        return Char_and_Length.definition(self) + "PyObject *tmp_%s;\n" % self.name

    def python_name(self):
        return self.length

    def byref(self):
        return "&%s" % self.length

    def pre_call(self, method):
        return """tmp_%s = PyString_FromStringAndSize(NULL, %s);
PyString_AsStringAndSize(tmp_%s, &%s, &%s);
""" % (self.name, self.length, self.name, self.name, self.length)

    def to_python_object(self):
        return """ _PyString_Resize(&tmp_%s, func_return); \npy_result = tmp_%s;\n""" % (
            self.name, self.name)

class TDB_DATA_P(Char_and_Length_OUT):
    def __init__(self, name, type):
        Type.__init__(self, name, type)

    def definition(self):
        return Type.definition(self)

    def byref(self):
        return "%s.dptr, &%s.dsize" % (self.name, self.name)

    def pre_call(self, method):
        return ''

    def call_arg(self):
        return Type.call_arg(self)

    def to_python_object(self):
        return "py_result = PyString_FromStringAndSize((char *)%s->dptr, %s->dsize);\ntalloc_free(%s);" % (
            self.name, self.name, self.name)

class TDB_DATA(TDB_DATA_P):
    def to_python_object(self):
        return "py_result = PyString_FromStringAndSize((char *)%s.dptr, %s.dsize);\n" % (
            self.name, self.name)

class Void(Type):
    buidstr = ''

    def __init__(self, *args):
        Type.__init__(self, None, 'void *')

    def definition(self):
        return ''

    def to_python_object(self):
        return "Py_INCREF(Py_None); py_result = Py_None;\n"

    def call_arg(self):
        return "NULL"

    def byref(self):
        return None

    def assign(self, call, method):
        ## We dont assign the result to anything
        return "%s;\n" % call

class Wrapper(Type):
    """ This class represents a wrapped C type """
    sense = 'IN'

    def to_python_object(self):
        return ''

    def definition(self):
        return "py%s *%s;\n" % (self.type, self.name)

    def call_arg(self):
        return "%s->base" % self.name

    def assign(self, call, method):
        args = dict(name = self.name, call = call, type = self.type,
                    error = method.error_condition())
        result = """{
       Object returned_object = (Object)%(call)s;

       if(!returned_object) {
         PyErr_Format(PyExc_RuntimeError,
                    "Failed to create object %(type)s");
         %(error)s;
       };

       %(name)s = (py%(type)s *)new_class_wrapper(returned_object);
       %(name)s->base = (%(type)s)returned_object;
}
""" % args

        if "BORROWED" in self.attributes:
            result += "talloc_increase_ref_count(%(name)s->base);\n" % args

        return result

    def to_python_object(self):
        return "py_result = (PyObject *)%(name)s;\n" % self.__dict__


class StructWrapper(Wrapper):
    """ A wrapper for struct classes """
    def assign(self, call, method):
        args = dict(name = self.name, call = call, type = self.type)
        result = """
%(name)s = (py%(type)s *)PyObject_New(py%(type)s, &%(type)s_Type);
%(name)s->base = %(call)s;
""" % args

        if "BORROWED" in self.attributes:
            result += "talloc_increase_ref_count(%(name)s->base);\n" % args

        return result

    def byref(self):
        return "&%s" % self.name

class PointerStructWrapper(StructWrapper):
    def __init__(self, name, type):
        type = type.split()[0]
        Wrapper.__init__(self,name, type)

class Timeval(Type):
    """ handle struct timeval values """
    interface = 'numeric'
    buidstr = 'f'

    def definition(self):
        return "float %(name)s_flt; struct timeval %(name)s;\n" % self.__dict__

    def byref(self):
        return "&%s_flt" % self.name

    def pre_call(self, method):
        return "%(name)s.tv_sec = (int)%(name)s_flt; %(name)s.tv_usec = (%(name)s_flt - %(name)s.tv_sec) * 1e6;\n" % self.__dict__

    def to_python_object(self):
        return "py_result = PyFloat_FromDouble((double)(%(name)s.tv_sec) + %(name)s.tv_usec);\n" % dict(name = self.name)

type_dispatcher = {
    "IN char *": String,
    "IN unsigned char *": String,
    "char *": String,

    "OUT char *": StringOut,
    "unsigned int": Integer,
    'int': Integer,
    'char': Char,
    'void': Void,
    'void *': Void,

    'TDB_DATA *': TDB_DATA_P,
    'TDB_DATA': TDB_DATA,
    'uint64_t': Integer,
    'int64_t': Integer,
    'unsigned long int': Integer,
    'struct timeval': Timeval,
    }

def dispatch(name, type):
    type_components = type.split()
    attributes = set()

    if type_components[0] == 'BORROWED':
        type_components.pop(0)
        attributes.add("BORROWED")
    
    type = " ".join(type_components)
    result = type_dispatcher[type](name, type)
    result.attributes = attributes

    return result

class Method:
    def __init__(self, class_name, base_class_name, method_name, args, return_type):
        self.name = method_name
        self.class_name = class_name
        self.base_class_name = base_class_name
        self.args = []
        self.definition_class_name = class_name
        for type,name in args:
            self.add_arg(type, name)

        try:
            self.return_type = dispatch('func_return', return_type)
            self.return_type.attributes.add("OUT")
        except KeyError:
            ## Is it a wrapped type?
            log("Unable to handle return type %s.%s %s" % (self.class_name, self.name, return_type))
            pdb.set_trace()
            self.return_type = Void()

    def clone(self, new_class_name):
        result = self.__class__(new_class_name, self.base_class_name, self.name,
                                [], 'void *')
        result.args = self.args
        result.return_type = self.return_type
        result.definition_class_name = self.definition_class_name

        return result

    def write_local_vars(self, out):
        kwlist = """static char *kwlist[] = {"""
        for type in self.args:
            python_name = type.python_name()
            if python_name:
                kwlist += '"%s",' % python_name
        kwlist += ' NULL};\n'

        for type in self.args:
            out.write(type.definition());

        parse_line = ''
        for type in self.args:
            if type.buidstr:
                parse_line += type.buidstr

        if parse_line:
            ## Now parse the args from python objects
            out.write(kwlist)
            out.write("\nif(!PyArg_ParseTupleAndKeywords(args, kwds, \"%s\", kwlist, " % parse_line)
            tmp = []
            for type in self.args:
                ref = type.byref()
                if ref:
                    tmp.append(ref)

            out.write(",".join(tmp))
            out.write("))\n %s;\n\n" % self.error_condition())

    def error_condition(self):
        return "return NULL";

    def write_definition(self, out):
        self.comment(out)
        out.write("""static PyObject *py%(class_name)s_%(method)s(py%(class_name)s *self, PyObject *args, PyObject *kwds) {\nPyObject *returned_result, *py_result;\n""" % dict(method = self.name, class_name = self.class_name))
        out.write(self.return_type.definition())

        self.write_local_vars( out);

        ## Precall preparations
        out.write(self.return_type.pre_call(self))
        for type in self.args:
            out.write(type.pre_call(self))

        call = "((%s)self->base)->%s(((%s)self->base)" % (self.definition_class_name, self.name, self.definition_class_name)
        tmp = ''
        for type in self.args:
            tmp += ", " + type.call_arg()

        call += "%s)" % tmp

        ## Now call the wrapped function
        out.write(self.return_type.assign(call, self))

        ## Now assemble the results
        results = [self.return_type.to_python_object()]
        for type in self.args:
            if type.sense == 'OUT':
                results.append(type.to_python_object())

        ## Make a tuple of results and pass them back
        if len(results)>1:
            out.write("returned_result = PyList_New(0);\n")
            for result in results:
                out.write(result)
                out.write("PyList_Append(returned_result, py_result); Py_DECREF(py_result);\n");
            out.write("return returned_result;\n")
        else:
            out.write(results[0])
            ## This useless code removes compiler warnings
            out.write("returned_result = py_result;\nreturn returned_result;\n");

        out.write("};\n\n")

    def add_arg(self, type, name):
        try:
            t = type_dispatcher[type](name, type)
        except KeyError:
            log( "Unable to handle type %s.%s %s" % (self.class_name, self.name, type))
            return

        ## Here we collapse char * + int type interfaces into a
        ## coherent string like interface.
        if self.args and t.interface == 'integer' and \
                self.args[-1].interface == 'string':
            previous = self.args[-1]

            ## We make a distinction between IN variables and OUT
            ## variables
            if previous.sense == 'OUT':
                cls = Char_and_Length_OUT
            else:
                cls = Char_and_Length

            self.args[-1] = cls(
                previous.name,
                previous.type,
                name, type)
        else:
            self.args.append(t)

    def comment(self, out):
        out.write("\n/********************************************************\nAutogenerated wrapper for function:\n")
        out.write(self.return_type.type+" "+self.class_name+"."+self.name+"(")
        for type in self.args:
            out.write(type.definition())
        out.write(");\n")
        out.write("********************************************************/\n")

    def prototype(self, out):
        out.write("""static PyObject *py%(class_name)s_%(method)s(py%(class_name)s *self, PyObject *args, PyObject *kwds);\n""" % dict(method = self.name, class_name = self.class_name))

class ConstructorMethod(Method):
    ## Python constructors are a bit different than regular methods
    def prototype(self, out):
        out.write("""static int py%(class_name)s_init(py%(class_name)s *self, PyObject *args, PyObject *kwds);\n""" % dict(method = self.name, class_name = self.class_name))

    def write_destructor(self, out):
        free = """
    if(self->base) {
        talloc_free(self->base);
        self->base=NULL;
    };
"""
        out.write("""static void
%(class_name)s_dealloc(py%(class_name)s *self) {
%(free)s
};\n
""" % dict(class_name = self.class_name, free=free))

    def error_condition(self):
        return "return -1";

    def write_definition(self, out):
        out.write("""static int py%(class_name)s_init(py%(class_name)s *self, PyObject *args, PyObject *kwds) {\n""" % dict(method = self.name, class_name = self.class_name))

        self.write_local_vars(out)

        ## Precall preparations
        for type in self.args:
            out.write(type.pre_call(self))

        ## Now call the wrapped function
        out.write("\nself->base = CONSTRUCT(%s, %s, %s, NULL" % (self.class_name,
                                                                 self.definition_class_name,
                                                                 self.name))
        tmp = ''
        for type in self.args:
            tmp += ", " + type.call_arg()

        out.write("""%s);
  if(!self->base) {
    PyErr_Format(PyExc_IOError, "Unable to construct class %s");
    return -1;
  };""" % (tmp, self.class_name))

        out.write("  return 0;\n};\n\n")

class StructConstructor(ConstructorMethod):
    """ A constructor for struct wrappers - basically just allocate
    memory for the struct. 
    """
    def write_definition(self, out):
        out.write("""static int py%(class_name)s_init(py%(class_name)s *self, PyObject *args, PyObject *kwds) {\n""" % dict(method = self.name, class_name = self.class_name))

        out.write("\nself->base = NULL;\n")
        out.write("  return 0;\n};\n\n")

    def write_destructor(self, out):
        out.write("""static void
%(class_name)s_dealloc(py%(class_name)s *self) {
if(self->base) talloc_free(self->base);
};\n
""" % dict(class_name = self.class_name))

class ClassGenerator:
    def __init__(self, class_name, base_class_name, module):
        self.class_name = class_name
        self.methods = []
        self.module = module
        self.constructor = None
        self.base_class_name = base_class_name

    def clone(self, new_class_name):
        """ Creates a clone of this class - usefull when implementing
        class extensions
        """
        result = ClassGenerator(new_class_name, self.class_name, self.module)
        result.constructor = self.constructor.clone(new_class_name)
        result.methods = [ x.clone(new_class_name) for x in self.methods ]

        return result

    def add_method(self, method_name, args, return_type):
        self.methods.append(Method(self.class_name, self.base_class_name,
                                   method_name, args, return_type))

    def add_constructor(self, method_name, args, return_type):
        self.constructor = ConstructorMethod(self.class_name, self.base_class_name,
                                             method_name, args, return_type)

    def struct(self,out):
        out.write("""\ntypedef struct {
  PyObject_HEAD
  %(class_name)s base;
} py%(class_name)s; \n
""" % dict(class_name=self.class_name))

    def code(self, out):
        if not self.constructor:
            raise RuntimeError("No constructor found for class %s" % self.class_name)

        self.constructor.write_destructor(out)
        self.constructor.write_definition(out)

        for m in self.methods:
            m.write_definition(out)

    def initialise(self):
        return "python_wrappers[TOTAL_CLASSES].class_ref = (Object)&__%s;\n" \
            "python_wrappers[TOTAL_CLASSES++].python_type = &%s_Type;\n" % (
            self.class_name, self.class_name)

    def PyMethodDef(self, out):
        out.write("static PyMethodDef %s_methods[] = {\n" % self.class_name)
        for method in self.methods:
            method_name = method.name
            out.write('     {"%s",(PyCFunction)py%s_%s, METH_VARARGS|METH_KEYWORDS, ""},\n' % (
                    method_name,
                    self.class_name,
                    method_name))
        out.write("     {NULL}  /* Sentinel */\n};\n")

    def prototypes(self, out):
        """ Write prototype suitable for .h file """
        out.write("""staticforward PyTypeObject %s_Type;\n""" % self.class_name)
        self.constructor.prototype(out)
        for method in self.methods:
            method.prototype(out)

    def PyTypeObject(self, out):
        out.write("""
static PyTypeObject %(class)s_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "%(module)s.%(class)s",               /* tp_name */
    sizeof(py%(class)s),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)%(class)s_dealloc,/* tp_dealloc */
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
    "%(class)s",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    %(class)s_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)py%(class)s_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
""" % {'class':self.class_name, 'module': self.module.name})

class StructGenerator(ClassGenerator):
    """ A wrapper generator for structs """
    def __init__(self, class_name, base_class_name, module):
        self.class_name = class_name
        self.methods = []
        self.module = module
        self.constructor = StructConstructor(class_name, base_class_name, 
                                             'Con', [], "void")
        self.base_class_name = base_class_name

    def struct(self, out):
        out.write("""\ntypedef struct {
  PyObject_HEAD
  %(class_name)s *base;
} py%(class_name)s; \n
""" % dict(class_name=self.class_name))

    def initialise(self):
        return ''

class parser:
    class_re = re.compile(r"^\s*CLASS\(([A-Z_a-z]+)\s*,\s*([A-Z_a-z]+)\)")
    method_re = re.compile(r"^\s*([0-9A-Z_a-z ]+\s+\*?)METHOD\(([A-Z_a-z]+),\s*([A-Z_a-z]+),?")
    arg_re = re.compile(r"\s*([0-9A-Z a-z_]+\s+\*?)([0-9A-Za-z_]+),?")
    constant_re = re.compile(r"#define\s+([A-Z_0-9]+)\s+")
    struct_re = re.compile(r"typedef struct\s+([A-Z_a-z0-9]+)\s+{")
    current_class = None

    def __init__(self, module):
        self.module = module

    def add_class(self, class_name, base_class_name, class_type, handler):
        try:
            self.current_class = self.module.classes[base_class_name].clone(class_name)
        except KeyError:
            log("Base class %s is not defined !!!!" % base_class_name)
            self.current_class = class_type(class_name, base_class_name, self.module)

        ## Now add the new class to the module object
        self.module.add_class(self.current_class, handler)

    def parse(self, filename):
        self.module.headers += '#include "%s"\n' % filename
        fd = open(filename)

        while 1:
            line = fd.readline()
            if not line: break

            m = self.constant_re.search(line)
            if m:
                ## We need to determine if it is a string or integer
                if re.search('"', line):
                    ## Its a string
                    self.module.add_constant(m.group(1), 'string')
                else:
                    self.module.add_constant(m.group(1), 'numeric')

            ## Wrap structs
            m = self.struct_re.search(line)
            if m:
                class_name = m.group(1)
                base_class_name = "Object"
                ## Structs may be refered to as a pointer or absolute
                ## - its the same thing ultimatley
                self.add_class(class_name, base_class_name, StructGenerator, StructWrapper)
                type_dispatcher["%s *" % class_name] = PointerStructWrapper
                continue

            m = self.class_re.search(line)
            if m:
                ## We need to make a new class now... We basically
                ## need to build on top of previously declared base
                ## classes - so we try to find base classes, clone
                ## them if possible:
                class_name = m.group(1)
                base_class_name = m.group(2)
                self.add_class(class_name, base_class_name, ClassGenerator, Wrapper)
                continue

            m = self.method_re.search(line)
            if m:
                args = []
                method_name = m.group(3)
                return_type = m.group(1).strip()
                ## Now parse the args
                offset = m.end()
                while 1:
                    m = self.arg_re.match(line[offset:])
                    if not m:
                        ## Allow multiline definitions if there is \\
                        ## at the end of the line
                        if line.strip().endswith("\\"):
                            line = fd.readline()
                            offset = 0
                            if line:
                                continue

                        break

                    offset += m.end()
                    args.append([m.group(1).strip(), m.group(2).strip()])

                if return_type == self.current_class.class_name and \
                        not self.current_class.constructor:
                    self.current_class.add_constructor(method_name, args, return_type)
                else:
                    self.current_class.add_method(method_name, args, return_type)

    def write(self, out):
        self.module.write(out)

p = parser(Module("pyaff4"))
for arg in sys.argv[1:]:
    p.parse(arg)

p.write(sys.stdout)
