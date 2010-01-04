import pyaff4, os
import time,pdb
time.sleep(1)

""" This program demonstrates how to install a new RDFValue handler.

We need to define a python class with the minimum required methods. In
order for the resolver to be able to use a python object we need to
wrap it in a proxy. The proxy class builds a C class which proxies the
python object - this means that when a c function calls the object's
method, the proxy will translate it into a python call for the class
method.

Once we register the class as an RDFValue implementation, we can obtain
new instances from the resolver by naming its dataType.
"""

oracle = pyaff4.Resolver(pyaff4.RESOLVER_MODE_NONPERSISTANT)

class RDFSpecial:
    """ This is a do nothing class to demonstate python serialization.

    We demonstrate it by encoding the value using hex (its basically
    equivalent to xsd:hexBinary)
    """
    # Our dataType is the name which we will use to serialise this
    # type - this field is mandatory and must be globally unique
    dataType = pyaff4.PREDICATE_NAMESPACE + "RDFSpecial"
    value = ''

    def encode(self):
        """ This method will be called when we are required to encode
        ourselves to the database.

        Since the database is binary safe its more efficient for us to
        just encode directly. The database is also not meant to be
        transportable so we dont care about endianess or things like
        that.
        """
        return self.value

    def decode(self, data):
        """ This method is called when we decode ourselves from the database. """
        self.value = data

    def serialise(self):
        """ This method is called when we need to write ourselves to
        the RDF serialization.

        The RDF serialization restricts the character range
        allowed. For serialization we therefore need to encode into
        something which is url safe.
        """
        return self.value.encode("hex")

    def parse(self, serialised):
        """ This method is called when we need to parse ourselves from
        the rdf serialization.

        This is the opposite of the serialization.
        """
        self.value = serialised.decode("hex")

    def set(self, value):
        """ This is a high level interface for users to set the
        contents of this object.

        You can make any number of functions here, these are not used
        by the underlying C implementation and therefore not callable
        from C code - but you can still call them from python.
        """
        print "Setting value"
        self.value = value

## Register the class for handling RDF objects
oracle.register_rdf_value_class(pyaff4.ProxiedRDFValue(RDFSpecial))

## We can obtain a new value of this type by nameing the dataType:
value = oracle.new_rdfvalue(RDFSpecial.dataType)

## Note that this method is called in the proxy class, but ends up
## calling the RDFSpecial class instance:
value.set("foobar")

## Make up some URI to attach to:
urn = pyaff4.RDFURN()
urn.set("hello")

## We make up an attribute within the aff4 namespace
attr = pyaff4.PREDICATE_NAMESPACE + "sample_attribute"

## Now we can use it in the resolver as normal
oracle.set_value(urn, attr, value)

## Now set the same value using an alternative dataType:
value2 = pyaff4.XSDString()
value2.set("foobar")

## note that the same attribute can have multiple values encoded using
## different dataTypes:
oracle.add_value(urn, attr, value2)

## Print out all the attributes
iter = oracle.get_iter(urn, attr)
while 1:
    obj = oracle.iter_next_alloc(iter)
    if not obj: break

    print "%s - Resolved value: %r" % (obj, obj.encode())

## Now we want to write everything to an RDF serialization. This is
## done in a number of steps:
##
## 1) A FileLikeObject is created to write the data to
## 2) An RDFSerializer is created for this FileLikeObject
## 3) The required URNs are serialised in turn
## 4) The serialiser is closed.
## 5) The FileLikeObject is closed.

## Where to store the test data:
output_urn = pyaff4.RDFURN()
output_urn.set("/tmp/output.txt")

## Make sure its cleaned
try:
    os.unlink(output_urn.parser.query)
except: pass

## Create the output file
output = oracle.open(output_urn,"w")

## Create the serialiser for it
serialiser = pyaff4.RDFSerializer(urn.value, output)

## This can be repeated for the required number of URIs
serialiser.serialize_urn(urn)

## This finished the serialisation and cleans up.
serialiser.close()

## Flush the output file
output.close()

## Verify the data
print open(output_urn.parser.query).read()
