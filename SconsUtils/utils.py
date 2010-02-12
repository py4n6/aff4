import os, sys, re, pdb
import distutils.sysconfig as sysconfig
import distutils.util
import platform

# taken from scons wiki
def CheckPKGConfig(context, version):
    context.Message( 'Checking for pkg-config version > %s... ' % version)
    ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
    context.Result( ret )
    return ret

def CheckFramework(context, name):
    ret = 0
    if (platform.system().lower() == 'darwin'):
        context.Message( '\nLooking for framework %s... ' % name )
        lastFRAMEWORKS = context.env['FRAMEWORKS']
        context.env.Append(FRAMEWORKS = [name])
        ret = context.TryLink("""
              int main(int argc, char **argv) {
                return 0;
              }
              """, '.c')
        if not ret:
            context.env.Replace(FRAMEWORKS = lastFRAMEWORKS
)

    return ret

def CheckFink(context):
    context.Message( 'Looking for fink... ')
    prog = context.env.WhereIs('fink')
    if prog:
        ret = 1
        prefix = prog.rsplit(os.sep, 2)[0]
        context.env.Append(LIBPATH = [prefix + os.sep +'lib'],
                           CPPPATH = [prefix + os.sep +'include'])
        context.Message( 'Adding fink lib and include path')
    else:
        ret = 0
        
    context.Result(ret)    
    return int(ret)

def CheckMacports(context):
    context.Message( 'Looking for macports... ')
    prog = context.env.WhereIs('port')
    if prog:
        ret = 1
        prefix = prog.rsplit(os.sep, 2)[0]
        context.env.Append(LIBPATH = [prefix + os.sep + 'lib'],
                           CPPPATH = [prefix + os.sep + 'include'])
        context.Message( 'Adding port lib and include path')
    else:
        ret = 0
        
    context.Result(ret)    
    return int(ret)

# TODO: We should use the scons one instead
def CheckLib(context, name):
    context.Message( 'Looking for lib %s... ' % name )
    lastLIBS = context.env['LIBS']
    context.env.Append(LIBS = [name])
    ret = context.TryLink("""
              int main(int argc, char **argv) {
                return 0;
              }
              """,'.c')
    if not ret:
        context.env.Replace(LIBS = lastLIBS)

    return ret

def ConfigPKG(context, name):
    context.Message( '\nUsing pkg-config for %s... ' % name )
    ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
    context.Result(  ret )
    if ret:
        context.env.ParseConfig('pkg-config --cflags --libs \'%s\'' % name)
    return int(ret)

def CheckPKG(context, name):
    context.Message( 'Checking for %s... ' % name )
    if platform.system().lower() == 'windows':
        return 0 
    ret = 1
    if not CheckFramework(context, name):
        if not ConfigPKG(context, name.lower()):
            ret = CheckLib(context, name) 

    context.Result(ret)
    return int(ret)


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

compile_source_message = '%sCompiling %s==> %s$SOURCE%s' % \
   (colors['blue'], colors['purple'], colors['yellow'], colors['end'])

compile_shared_source_message = '%sCompiling shared %s==> %s$SOURCE%s' % \
   (colors['blue'], colors['purple'], colors['yellow'], colors['end'])

compile_python_source_message = '%sCompiling python module %s==> %s$SOURCE%s' % \
   (colors['blue'], colors['purple'], colors['yellow'], colors['end'])

link_program_message = '%sLinking Program %s==> %s$TARGET%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

link_library_message = '%sLinking Static Library %s==> %s$TARGET%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

ranlib_library_message = '%sRanlib Library %s==> %s$TARGET%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

link_shared_library_message = '%sLinking Shared Library %s==> %s$TARGET%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

link_python_module_message = '%sLinking Native Python module %s==> %s${TARGET}%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

java_library_message = '%sCreating Java Archive %s==> %s$TARGET%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

def install_colors(args):
    """ Installs colors into an environment """
    args.update(dict( CXXCOMSTR = compile_source_message,
                      CCCOMSTR = compile_source_message,
                      SHCCCOMSTR = compile_shared_source_message,
                      SHCXXCOMSTR = compile_shared_source_message,
                      ARCOMSTR = link_library_message,
                      RANLIBCOMSTR = ranlib_library_message,
                      SHLINKCOMSTR = link_shared_library_message,
                      LINKCOMSTR = link_program_message,
                      JARCOMSTR = java_library_message,
                      JAVACCOMSTR = compile_source_message,))

import optparse

### This workaround is because scons does not provide access to the
### parser, and by setting Help() we are unable to generate the option
### listing from AddOption
my_parser = optparse.OptionParser()

import SCons.Script.Main as Main
import SCons.Script as Script

def add_option(arg, option, *args, **kwargs):
    opt = "--%s" % option
    Main.AddOption(opt, *args, **kwargs)
    my_parser.add_option(opt, *args, **kwargs)

    arg[option] = Main.GetOption(option)

def generate_help(vars, env):
    Script.Help("AFF4 build system configuration.\n\nFollowing are compile time options:\n")
    Script.Help(my_parser.format_help())
    Script.Help("\nThe following variables can be used on the command line:\n")
    Script.Help(vars.GenerateHelpText(env))

HEADERS = {}

def check_type(conf, types):
    header = None
    for t in types:
        if ':' in t:
            t, header = t.split(':')
        define = "HAVE_" + t.upper().replace(".","_")

        HEADERS[define] = conf.CheckType(t, includes="#include <%s>\n" % header)

def check(type, conf, headers):
    for header in headers:
        if ":" in header:
            define, header = header.split(':')
        else:
            if "/" in header:
                tmp = header.split("/")[-1]
            else:
                tmp = header

            define = "HAVE_" + tmp.upper().replace(".","_")

        global HEADERS
        if type == 'header':
            HEADERS[define] = conf.CheckCHeader(header)
        elif type == 'func':
            HEADERS[define] = conf.CheckFunc(header)
        elif type == 'lib':
            HEADERS[define] = conf.CheckLib(header)

## Build the config.h file
def config_h_build(target, source, env):
    config_h_defines = env.Dictionary()
    config_h_defines.update(env.config.__dict__)
    warn("Generating config.h")

    for a_target, a_source in zip(target, source):
        config_h = file(str(a_target), "w")
        config_h_in = file(str(a_source), "r")
        config_h.write(config_h_in.read() % config_h_defines)
        config_h_in.close()


    keys = HEADERS.keys()
    keys.sort()

    for k in keys:
        if HEADERS[k]:
            config_h.write("#define %s 1\n" % k)
        else:
            config_h.write("/** %s unset */\n" % k)

    config_h.close()

import SCons.Environment

class ExtendedEnvironment(SCons.Environment.Environment):
    """ Implementation from Richard Levitte email to
    org.tigris.scons.dev dated Jan 26, 2006 7:05:10 am."""
    python_cppflags = distutils.util.split_quoted(
        "-I"+sysconfig.get_python_inc())

    def PythonModule(self, libname, lib_objs=[], **kwargs):
        """ This builds a python module which is almost a library but
        is sometimes named differently.

        We have two modes - a cross compile mode where we do our best
        to guess the flags. In the native mode we can get the required
        flags directly from distutils.
        """
        platform = self.subst('$PLATFORM')
        shlib_pre_action = None
        shlib_suffix = distutils.util.split_quoted(
            sysconfig.get_config_var('SO'))
        shlib_post_action = None
        cppflags = distutils.util.split_quoted(
            "-I"+sysconfig.get_python_inc())
        shlink_flags = ['-laff4']
        install_dest = distutils.util.split_quoted(
            os.path.join(
                sysconfig.get_config_var('BINLIBDEST'),os.path.dirname(libname)))

        flags = distutils.util.split_quoted(
            sysconfig.get_config_var('LDSHARED'))

        ## For some stupid reason they include the compiler in LDSHARED
        shlink_flags.extend([x for x in flags if 'gcc' not in x])

        shlink_flags.append(sysconfig.get_config_var('LOCALMODLIBS'))

        ## TODO cross compile mode
        kwargs['LIBPREFIX'] = ''
        kwargs['CPPFLAGS'] = cppflags
        kwargs['SHLIBSUFFIX'] = shlib_suffix
        kwargs['SHLINKFLAGS'] = shlink_flags

        if not self.config.V:
            kwargs['SHCCCOMSTR'] = compile_python_source_message
            kwargs['SHLINKCOMSTR'] = link_python_module_message

        lib = self.SharedLibrary(libname,lib_objs,
                                 **kwargs)

        ## Install it to the right spot
        self.Install(install_dest, lib)
        self.Alias('install', install_dest)

        return lib

    def VersionedSharedLibrary(self, libname, libversion, lib_objs=[]):
        """ This creates a version library similar to libtool.

        We name the library with the appropriate soname.
        """
        platform = self.subst('$PLATFORM')
        shlib_pre_action = None
        shlib_suffix = self.subst('$SHLIBSUFFIX')
        shlib_post_action = None
        shlink_flags = SCons.Util.CLVar(self.subst('$SHLINKFLAGS'))

        if platform == 'posix':
            shlib_post_action = [ 'rm -f $TARGET', 'ln -s ${SOURCE.file} $TARGET' ]
            shlib_post_action_output_re = [ '%s\\.[0-9\\.]*$' % re.escape(shlib_suffix), shlib_suffix ]
            shlib_suffix += '.' + libversion
            shlink_flags += [ '-Wl,-Bsymbolic', '-Wl,-soname=${LIBPREFIX}%s%s' % (
                    libname, shlib_suffix) ]
        elif platform == 'aix':
            shlib_pre_action = [ "nm -Pg $SOURCES > ${TARGET}.tmp1", "grep ' [BDT] ' < ${TARGET}.tmp1 > ${TARGET}.tmp2", "cut -f1 -d' ' < ${TARGET}.tmp2 > ${TARGET}", "rm -f ${TARGET}.tmp[12]" ]
            shlib_pre_action_output_re = [ '$', '.exp' ]
            shlib_post_action = [ 'rm -f $TARGET', 'ln -s $SOURCE $TARGET' ]
            shlib_post_action_output_re = [ '%s\\.[0-9\\.]*' % re.escape(shlib_suffix), shlib_suffix ]
            shlib_suffix += '.' + libversion
            shlink_flags += ['-G', '-bE:${TARGET}.exp', '-bM:SRE']
        elif platform == 'cygwin':
            shlink_flags += [ '-Wl,-Bsymbolic', '-Wl,--out-implib,${TARGET.base}.a' ]
        elif platform == 'darwin':
            shlib_suffix = '.' + libversion + shlib_suffix
            shlink_flags += [ '-dynamiclib', '-current-version %s' % libversion ]

        lib = self.SharedLibrary(libname,lib_objs,
                                 SHLIBSUFFIX=shlib_suffix,
                                 SHLINKFLAGS=shlink_flags)

        if shlib_pre_action:
            shlib_pre_action_output = re.sub(shlib_pre_action_output_re[0], shlib_pre_action_output_re[1], str(lib[0]))
            self.Command(shlib_pre_action_output, [ lib_objs ], shlib_pre_action)
            self.Depends(lib, shlib_pre_action_output)

        if shlib_post_action:
            shlib_post_action_output = re.sub(shlib_post_action_output_re[0], shlib_post_action_output_re[1], str(lib[0]))
            self.Command(shlib_post_action_output, lib, shlib_post_action)

        return lib

    def InstallVersionedSharedLibrary(self, destination, lib):
        platform = self.subst('$PLATFORM')
        shlib_suffix = self.subst('$SHLIBSUFFIX')
        shlib_install_pre_action = None
        shlib_install_post_action = None

        if platform == 'posix':
            shlib_post_action = [ 'rm -f $TARGET', 'ln -s ${SOURCE.file} $TARGET' ]
            shlib_post_action_output_re = [ '%s\\.[0-9\\.]*$' % re.escape(shlib_suffix), shlib_suffix ]
            shlib_install_post_action = shlib_post_action
            shlib_install_post_action_output_re = shlib_post_action_output_re

        ilib = self.Install(destination,lib)

        if shlib_install_pre_action:
            shlib_install_pre_action_output = re.sub(shlib_install_pre_action_output_re[0], shlib_install_pre_action_output_re[1], str(ilib[0]))
            self.Command(shlib_install_pre_action_output, ilib, shlib_install_pre_action)
            self.Depends(shlib_install_pre_action_output, ilib)

        if shlib_install_post_action:
            shlib_install_post_action_output = re.sub(shlib_install_post_action_output_re[0], shlib_install_post_action_output_re[1], str(ilib[0]))
            self.Command(shlib_install_post_action_output, ilib, shlib_install_post_action)
