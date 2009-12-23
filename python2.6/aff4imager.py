#!/usr/bin/python

""" This is a python implementation of the aff4 imager. """
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

parser.add_option("-t", "--threads", default=2,
                  help="Number of threads to use")

parser.add_option('-v', '--verbosity', default=5,
                  help="Verbosity")

parser.add_option('-e', '--encrypt', default=False,
                  action = 'store_true',
                  help="Encrypt the image")

parser.add_option("-p", "--password", default='',
                  help='Use this password instead of prompting')

(options, args) = parser.parse_args()

