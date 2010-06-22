""" This is a test suite for pyaff4 """

import pyaff4
import unittest
import pickle

class TestResult:
    """ This class encapsulates a result and allows comparison with some other object """
    def __init__(self, result):
        self.result = result

    def __eq__(self, other):
        """ By default we just compare the string representation """
        return str(self.result) == str(other)


class TestCase:
    def __init__(self, context=None):
        """ The context if provided is a dictionary with previously
        calculated test results 
        """
        if context==None:
            context = {}

        self.context = context

    def store(self, result = None, key=None):
        if not key: key = self.__class__.__name__
        self.context[key] = result

    def record(self):
        """ This method is used to run this test and collect results
        to be saved in our context
        """

    def test(self, context):
        """ Run a test case against a context """
        self.record()
        for k,v in self.context.items():
            expected = context.get(k)
            if not v==expected:
                print("Test {0} failed. Got {1} should be {2}".format(k, v, expected))

class RDFTests(TestCase):
    """ Tests setting, serialising and parsing all the supported RDFValues """
    RDFClasses = [ ('XSDDatetime', pyaff4.XSDDatetime, 1277181726),
                   ('XSDString', pyaff4.XSDString, "hello world"),
                   ('Generic URL', pyaff4.RDFURN, "http://www.google.com"),
                   ('File URL', pyaff4.RDFURN, "/etc/password"),
                   ]

    def record(self):
        for key, obj, value in self.RDFClasses:
            obj = obj()
            obj.set(value)

            ## First serialise this
            serialized = obj.serialise()
            self.store(serialized, key=key)

            ## Now try to parse the serialized version and reserialise it
            obj.parse(serialized)
            serialised_again = obj.serialise()
            self.store(serialised_again, key = key + " parsed")

            assert serialised_again == serialized

## This is the new result context that we create
context = {}

## Build a list of all test cases to run now
test_cases = []
for cls_name in dir():
    try:
        cls = locals()[cls_name]
        if issubclass(cls, TestCase):
            test_cases.append(cls(context))
    except (AttributeError,TypeError): pass

## Record all the results now
for test in test_cases:
    test.record()

print context
