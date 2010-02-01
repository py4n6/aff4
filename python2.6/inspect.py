#!/usr/bin/python
""" Module to inspect the contents of an AFF4 volume """
import sys, pdb, os
import re, cmd, readline
import optparse
import pyaff4
import shlex
import fnmatch, time
import subprocess
import readline

time.sleep(1)

## Set more sane completer delimiters
readline.set_completer_delims(' \t\n`!@#$^&*()=+[{]}\\|;:\'",<>?')

## Configure colors
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

parser = optparse.OptionParser()
(options, args) = parser.parse_args()

## Load the specified args as volumes
oracle = pyaff4.Resolver()
VOLUMES = []
STREAMS = {}

for arg in args:
    volume = pyaff4.RDFURN()
    volume.set(arg)
    if oracle.load(volume):
        VOLUMES.append(volume)
        iter = oracle.get_iter(volume, pyaff4.AFF4_CONTAINS)
        while iter:
            urn = oracle.iter_next_alloc(iter)
            if not urn: break

            ## We store is as a simplified path
            path = "/%s%s" % (urn.parser.netloc, urn.parser.query)
            STREAMS[path] = urn

class Inspector(cmd.Cmd):
    prompt = "/:>"
    intro = "Inspecting the volumes %s. Type 'help' for help." % args
    CWD = "/"

    def cmdloop(self, *args):
        while 1:
            try:
                if not cmd.Cmd.cmdloop(self, *args):
                    break
            except KeyboardInterrupt,e:
                print "Type %sexit%s to exit, %shelp%s for help" % (
                    colors['yellow'],
                    colors['end'],
                    colors['cyan'],
                    colors['end'])

    def do_EOF(self, args):
        """ Exit this program """
        print "%sGoodbye%s" % (colors['yellow'],
                               colors['end'])
        return True

    def do_cd(self, line):
        args = shlex.split(line)
        if not args[0].startswith("/"):
            args[0] = self.CWD + args[0]

        try:
            potential_cd = os.path.normpath(args[0])
            if not potential_cd.endswith('/'):
                potential_cd += "/"

            for s in STREAMS:
                if s.startswith(potential_cd):
                    self.CWD = potential_cd
                    self.prompt = "%s:>" % self.CWD
                    return

            raise IOError("No such directory %s" % potential_cd)
        except IndexError:
            print "CWD: %s" % self.CWD

    def complete_cd(self, *args):
        return self.complete_stream(*args)

    def do_help(self, line):
        """ Provide help for commands """
        args = shlex.split(line)
        if not args:
            ## List all commands
            for k in dir(self):
                if k.startswith("do_"):
                    method = getattr(self, k)
                    print "%s%s%s: %s" % (colors['red'],
                                          k[3:],
                                          colors['end'],
                                          method.__doc__)
            return

        for arg in args:
            try:
                method = getattr(self, "do_%s" % arg)
            except AttributeError:
                print "%sError:%s Command %s%s%s not found" % (colors['red'],
                                                               colors['end'],
                                                               colors['yellow'],
                                                               arg,
                                                               colors['end'])

            print "%s: %s" % (arg, method.__doc__)

    def do_ls(self, line):
        """ List streams matching a glob expression """
        globs = shlex.split(line)
        if not globs: globs=[self.CWD]

        result = []

        for s in STREAMS:
            for g in globs:
                if fnmatch.fnmatch(s, g + "*"):
                    if s.startswith(self.CWD):
                        s = s[len(self.CWD):]

                    path = s.split("/")[0]
                    if path == s:
                        decoration = colors['blue']
                    else:
                        decoration = colors['end']

                    if path not in result:
                        print "%s%s%s" % (decoration, path, colors['end'])
                        result.append(path)

    def do_less(self, line):
        """ Read the streams specified and pipe them through the pager """
        globs = shlex.split(line)
        for s in STREAMS:
            for g in globs:
                if not g.startswith("/"):
                    g = self.CWD + g

                g = os.path.normpath(g)
                if fnmatch.fnmatch(s, g):
                    ## Try to open the stream
                    try:
                        fd = oracle.open(STREAMS[s], 'r')
                    except IOError, e:
                        raise

                    pager = os.environ.get("PAGER","less")
                    pipe=os.popen(pager,"w")

                    while 1:
                        data = fd.read(1024 * 1024)
                        if not data: break

                        pipe.write(data)

                    pipe.close()

    def complete_less(self, *args):
        return self.complete_stream(*args)

    def complete_stream(self, text, line, begidx, endidx):
        if not text.startswith("/"):
            text = self.CWD + text

        if not text:
            completions = STREAMS.keys()

        completions = [ text + f[len(text):].split("/")[0]
                        for f in STREAMS.keys()
                        if f.startswith(text)
                        ]

        return completions

    def do_pwd(self, line):
        print "CWD: %s" % self.CWD

    def do_exit(self, *args):
        """ Exits the program """
        return self.do_EOF(*args)

    def emptyline(self):
        return

    def onecmd(self, line):
        try:
            return cmd.Cmd.onecmd(self, line)
        except Exception,e:
            print "%sError:%s %s%s%s %s" % (colors['red'],
                                            colors['end'],
                                            colors['yellow'],
                                            e.__class__.__name__,
                                            colors['end'],
                                            e)

        return None

Inspector().cmdloop()
