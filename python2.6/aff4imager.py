#!/usr/bin/python
""" This is a python implementation of the aff4 imager. 

This file contains utility functions first, followed by an optparse
front end. This allows users to import this file as a module, or run
it directly.  """
import sys, pdb
import re

import time
time.sleep(1)

## Configure colors for pretty builds
colors = {}
colors['cyan']   = '\033[96m'
colors['purple'] = '\033[95m'
colors['blue']   = '\033[94m'
colors['green']  = '\033[92m'
colors['yellow'] = '\033[93m'
colors['red']    = '\033[91m'
colors['end']    = '\033[0m'

#If the output is not a terminal, remove the colors
if not sys.stdout.isatty():
   for key, value in colors.iteritems():
      colors[key] = ''

def error(msg):
   print "%s%s%s" % (colors['red'], msg, colors['end'])
   sys.exit(1)

def warn(msg):
   print "%s%s%s" % (colors['yellow'], msg, colors['end'])


class Renderer:
    def message(self, level, service, subject, message):
       sys.stdout.write("%s%s%s: %s\n" % (colors['blue'],subject.value,
                                          colors['end'], message))

def image(output_URI, options, fds):
    """ Copy the file like objects specified in the fds list into an
    output volume specified by its URI.
    """
    out_fd = oracle.open(output_URI, 'w')
    ## A file urn was specified
    if isinstance(out_fd, pyaff4.FileLikeObject):
        out_fd.cache_return()

        ## Make a volume object
        zip_fd = oracle.create(pyaff4.AFF4_ZIP_VOLUME, 'w')
        oracle.set_value(zip_fd.urn, pyaff4.AFF4_STORED, output_URI)

        zip_fd = zip_fd.finish()
    else:
        zip_fd = out_fd


    zip_urn = zip_fd.urn
    zip_fd.cache_return()

    for fd in fds:
        image_fd = oracle.create(pyaff4.AFF4_IMAGE, 'w')

        warn("New image %s%s" % (colors['cyan'], fd.urn.value))
        ## We want to make the image URI the same as the volume URI
        ## with the query stem of the source appended to it:
        image_fd.urn.set(zip_urn.value)
        image_fd.urn.add(fd.urn.parser.query)
        image_fd.set_workers(options.threads)
        oracle.set_value(image_fd.urn, pyaff4.AFF4_STORED, zip_urn)

        image_fd = image_fd.finish()

        while 1:
            data = fd.read(10 * 1024 * 1024)

            if not data: break

            image_fd.write(data)

        image_fd.close()

    zip_fd = oracle.open(zip_urn, 'w')
    zip_fd.close()

import optparse
import pyaff4

parser = optparse.OptionParser()
parser.add_option("-i", "--image", default=None,
                  action='store_true',
                  help = "Imaging mode")

parser.add_option("-m", "--max_size", default=0,
                  help = "Try to change volumes after the volume is bigger than this size")

parser.add_option("-o", "--output", default=None,
                  help="Create the output volume on this file or URL (using webdav)")

parser.add_option("", "--link", default="default",
                  help="Create a symbolic link to the image URN with this name")

parser.add_option("-D","--dump", default=None,
                  help="Dump out this stream (to --output or stdout)")

parser.add_option("","--chunks_in_segment", default=0, type='int',
                  help="Total number of chunks in each bevy")

parser.add_option("","--nocompress", default=False, action='store_true',
                  help="Do not Compress image")

## This is needed to support globbing for -l option
def vararg_callback(option, opt_str, value, parser):
    assert value is None
    value = []

    for arg in parser.rargs:
        # stop on --foo like options
        if arg[:2] == "--" and len(arg) > 2:
            break

        ## stop on -f like options
        if arg[:1] == "-" and len(arg) > 1:
            break

        value.append(arg)

    del parser.rargs[:len(value)]
    setattr(parser.values, option.dest, value)

parser.add_option("-l","--load", default=[], dest="load",
                  action = "callback", callback=vararg_callback,
                  help="Load this volume to prepopulate the resolver")

parser.add_option("-I", "--info", default=None,
                  action = 'store_true',
                  help="Information mode - dump all information in the resolver")

parser.add_option("-V", "--verify", default=None,
                  action = 'store_true',
                  help="Verify all signatures and hashes when in --info mode")

parser.add_option("-k", "--key", default=None,
                  help="Key file to use (in PEM format)")

parser.add_option("-c", "--cert", default=None,
                  help="Certificate file to use (in PEM format)")

parser.add_option("-t", "--threads", default=0, type='int',
                  help="Number of threads to use")

parser.add_option('-v', '--verbosity', default=5,
                  help="Verbosity")

parser.add_option('-e', '--encrypt', default=False,
                  action = 'store_true',
                  help="Encrypt the image")

parser.add_option("-p", "--password", default='',
                  help='Use this password instead of prompting')

(options, args) = parser.parse_args()

## Make a new oracle
#oracle = pyaff4.Resolver(pyaff4.RESOLVER_MODE_DEBUG_MEMORY)
oracle = pyaff4.Resolver()

## Now register the renderer as an output module
oracle.set_logger(pyaff4.ProxiedLogger(Renderer()))

if options.dump:
   output = options.output
   if not output:
      out_fd = sys.stdout

   urn = pyaff4.RDFURN()
   urn.set(options.dump)
   fd = oracle.open(urn, 'r')
   try:
      while 1:
         data = fd.read(1024* 1024)
         if not data: break

         out_fd.write(data)

   finally:
      fd.cache_return()


elif options.image:
    ## Imaging mode
    output = options.output
    if not output:
        error("Output file must be specified when in imaging mode")

    fds = []
    urn = pyaff4.RDFURN()
    try:
        for arg in args:
            urn.set(arg)
            fd = oracle.open(urn, 'r')
            fds.append(fd)
            if not isinstance(fd, pyaff4.FileLikeObject):
                error("%s is not a file like object" % arg)

        urn.set(output)
        image(urn, options, fds)
    finally:
        for fd in fds:
            fd.cache_return()
