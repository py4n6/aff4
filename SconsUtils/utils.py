import os, sys
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
   Exit(1)

def warn(msg):
   print "%s%s%s" % (colors['yellow'], msg, colors['end'])

compile_source_message = '%sCompiling %s==> %s$SOURCE%s' % \
   (colors['blue'], colors['purple'], colors['yellow'], colors['end'])

compile_shared_source_message = '%sCompiling shared %s==> %s$SOURCE%s' % \
   (colors['blue'], colors['purple'], colors['yellow'], colors['end'])

link_program_message = '%sLinking Program %s==> %s$TARGET%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

link_library_message = '%sLinking Static Library %s==> %s$TARGET%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

ranlib_library_message = '%sRanlib Library %s==> %s$TARGET%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

link_shared_library_message = '%sLinking Shared Library %s==> %s$TARGET%s' % \
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

## Build the config.h file
def config_h_build(target, source, env):
    config_h_defines = env.Dictionary()
    warn("Generating config.h")

    for a_target, a_source in zip(target, source):
        config_h = file(str(a_target), "w")
        config_h_in = file(str(a_source), "r")
        config_h.write(config_h_in.read() % config_h_defines)
        config_h_in.close()
        config_h.close()
