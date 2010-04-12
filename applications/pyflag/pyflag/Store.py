#!/usr/bin/env python
"""  This file implements a memory store for python objects.

A memory store is a single semi/persistant storage for object
collections which ensure that memory resources are not exhausted. We
store a maximum number of objects, for a maximum length of time. When
objects expire they are deleted.

Objects are taken and returned to the store by their clients. Doing
this will refresh their age. This ensures that frequently used objects
remain young and therefore do not perish.

The store is also thread safe as a single thread is allowed to access
the store at any one time.
"""

import thread,time,re
import pyflaglog

## A much faster and simpler implementation of the above
class FastStore:
    """ This is a cache which expires objects in oldest first manner. """
    def __init__(self, limit=50, max_size=0, kill_cb=None):
        self.age = []
        self.hash = {}
        self.limit = max_size or limit
        self.kill_cb = kill_cb
        self.id = time.time()

    def expire(self):
        while len(self.age) > self.limit:
            x = self.age.pop(0)
            ## Kill the object if needed
            if self.kill_cb:
                self.kill_cb(self.hash[x])

            del self.hash[x]

    def put(self,object, prefix='', key=None):
        """ Stores an object in the Store.  Returns the key for the
        object. If key is already supplied we use that instead - Note
        that we do not check that it doesnt already exist.
        """
        ## Push the item in:
        if not key:
            key = "%s%s" % (prefix,self.id)
            self.id += 1

        self.add(key, object)
        return key

    def add(self, urn, obj):
        self.hash[urn] = obj
        self.age.append(urn)
        self.expire()

    def get(self, urn):
        return self.hash[urn]

    def __contains__(self, obj):
        return obj in self.hash

    def __getitem__(self, urn):
        return self.hash[urn]

    def flush(self):
        if self.kill_cb:
            for x in self.hash.values():
                self.kill_cb(x)

        self.hash = {}
        self.age = []

## Store unit tests:
import unittest
import random, time

class StoreTests(unittest.TestCase):
    """ Store tests """
    def test01StoreExpiration(self):
        """ Testing store removes objects when full """
        s = Store(max_size = 5)
        keys = []
        for i in range(0,100):
            keys.append(s.put(i))

        self.assertRaises(KeyError, lambda : s.get(keys[0]))

        ## This should not raise
        s.get(keys[-1])

    def test02StoreRefresh(self):
        """ Test that store keeps recently gotten objects fresh """
        s = Store(max_size = 5)
        keys = []
        for i in range(0,5):
            keys.append(s.put(i))

        ## This should not raise because keys[0] should be refreshed
        ## each time its gotten
        for i in range(0,1000):
            s.get(keys[0])
            s.put(i)

    def test03Expire(self):
        """ Tests the expire mechanism """
        s = Store(max_size = 100)
        for i in range(0,5):
            s.put(i, key="test%s" % i)

        for i in range(0,5):
            s.put(i, key="tests%s" % i)

        s.expire("test\d+")
        ## Should have 5 "testsxxx" left
        self.assertEqual(len(s.creation_times),5)
