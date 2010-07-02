import sys, pdb
import os
import SconsUtils.utils as utils
import distutils.sysconfig
import platform

## Users should alter the configuration in this file
import config

from SconsUtils.utils import error, warn, add_option, generate_help, config_h_build
import SconsUtils.utils

## Options
#SetOption('implicit_cache', 1)

args = dict(CPPPATH='#include/', tools=['default', 'packaging'],
            package_version = '0.1',
            ENV = {'PATH' : os.environ['PATH'],
                   'TERM' : os.environ['TERM'],
                   'HOME' : os.environ['HOME']}
            )

## Add optional command line variables
vars = Variables()

vars.AddVariables(
   ('CC', 'The c compiler', 'gcc'),
   )

args['variables'] = vars

args['CFLAGS']=' -Ilibraptor '
args['LINKFLAGS']=''
## Make sure we only build for one architecture on darwin
if platform.system().lower() == 'darwin':
   args['CFLAGS'] += config.DARWIN_ARCHITECTURE
   args['LINKFLAGS'] += config.DARWIN_ARCHITECTURE

if config.V:
   args['CFLAGS'] += ' -Wall -g -O0 -D__DEBUG__ '
else:
   utils.install_colors(args)

if 'lock' in config.DEBUG:
   args['CFLAGS'] += ' -DAFF4_DEBUG_LOCKS '

elif 'resolver' in config.DEBUG:
   args['CFLAGS'] += ' -DAFF4_DEBUG_RESOLVER '

elif 'object' in config.DEBUG:
   args['CFLAGS'] += ' -DAFF4_DEBUG_OBJECT '

add_option(args, 'prefix',
           type='string',
           nargs=1,
           action='store',
           metavar='DIR',
           default=config.PREFIX,
           help='installation prefix')

add_option(args, 'mingw', action='store_true', default=False,
           help = 'Use mingw to cross compile windows binaries')

env = utils.ExtendedEnvironment(**args)
env.config = config

Progress(['-\r', '\\\r', '|\r', '/\r'], interval=5)

cppDefines = [
    ( '_FILE_OFFSET_BITS', 64),
    '_LARGEFILE_SOURCE',
    'GCC_HASCLASSVISIBILITY',
    ]

warnings = [
    'all',
#    'write-strings',
#    'shadow',
#    'pointer-arith',
#    'packed',
#    'no-conversion',
#	'no-unused-result',  (need a newer gcc for this?)
    ]

compileFlags = [
    '-fno-exceptions',
    '-fno-strict-aliasing',
    '-msse2',
    '-D_XOPEN_SOURCE=600',
#    '-fvisibility=hidden',
#    '-fomit-frame-pointer',
#    '-flto',
    ]

# add the warnings to the compile flags
compileFlags += [ ('-W' + warning) for warning in warnings ]

env['CCFLAGS'] = compileFlags
env['CPPDEFINES'] = cppDefines
env['INSTALLSTR'] = utils.installed_message

## Not working yet
if config.MINGW_XCOMPILE:
   import SconsUtils.crossmingw as crossmingw

   crossmingw.generate(env)

conf = Configure(env)
conf.AddTests({'CheckTypeSize': utils.CheckTypeSize})

## Check for different things
if not env.GetOption('clean') and not env.GetOption('help'):
   ## Sizeof
   SconsUtils.utils.check_size(conf, ["unsigned int","unsigned char",
                                      "unsigned long","unsigned short"])

   ## Headers
   SconsUtils.utils.check("header", conf, Split("""
standards.h stdint.h inttypes.h string.h strings.h sys/types.h STDC_HEADERS:stdlib.h
crypt.h dlfcn.h stdint.h stddef.h stdio.h errno.h stdlib.h unistd.h fuse.h
utime.h arpa/inet.h stdargs.h libewf.h
"""))

   ## Mandatory dependencies
   if not conf.CheckLibWithHeader('z', 'zlib.h','c'):
      error( 'You must install zlib-dev to build libaff4!')

   if config.MINGW_XCOMPILE:
      if not conf.CheckLib('libeay32'):
         error('You must have openssl header libraries. This is often packaged as libssl-dev')

   else:
   ## Make sure the openssl installation is ok
      if not conf.CheckLib('ssl') or not conf.CheckLib('crypto'):
         error('You must have openssl header libraries. This is often packaged as libssl-dev')

   for header in Split('openssl/aes.h openssl/bio.h openssl/evp.h openssl/hmac.h openssl/md5.h openssl/rand.h openssl/rsa.h openssl/sha.h openssl/pem.h'):
      if not conf.CheckHeader(header):
         error("Openssl installation seems to be missing header %s" % header)

   ## Optional stuff:
   ## Functions
   SconsUtils.utils.check("func", conf, Split("""
strerror strdup memmove mktime timegm utime utimes strlcpy strlcat setenv
unsetenv seteuid setegid setresuid setresgid chown chroot link readlink symlink
realpath lchown setlinebuf strcasestr strcasecmp strtok strtoll strtoull ftruncate initgroups
bzero memset dlerror dlopen dlsym dlclose socketpair vasprintf snprintf vsnprintf
asprintf vsyslog dup2 mkdtemp pread pwrite inet_ntoa inet_pton inet_ntop
inet_aton connect gethostbyname getifaddrs freeifaddrs crypt vsnprintf strnlen
ntohll
"""))

   ## va_copy is a macro on some platforms so we need to check it better
   SconsUtils.utils.check_build(conf, "va_copy", "HAVE_VA_COPY", """
#include <stdarg.h>
int main() {
  va_list ap1,ap2;
  va_copy(ap1,ap2);

  return 0;
}
""")

   ## Libraries
   SconsUtils.utils.check("lib", conf, Split("""
ewf curl afflib pthread HAVE_OPENSSL:ssl tsk3 regfi
"""))

   ## We dont want to actually link to tsk3 and regfi - just check
   ## that they exist:
   try:
      env._dict['LIBS'].remove('tsk3')
   except ValueError: pass

   try:
      env._dict['LIBS'].remove('regfi')
   except ValueError: pass

   ## libewf comes in two flavours a V2 API and an older API
   SconsUtils.utils.check_build(conf, "libewf_glob", "HAVE_EWF_V2_API", """
#include <libewf.h>

int main() {
  libewf_glob(NULL, 0, 0, NULL, NULL, NULL);
  libewf_glob_free(NULL, 0, NULL);

  return 0;
}
""")

   ## Types
   SconsUtils.utils.check_type(conf, Split("""
intptr_t:stdint.h uintptr_t:stdint.h ptrdiff_t:stddef.h ssize_t:unistd.h
"""))

env = conf.Finish()

generate_help(vars, env)

Export("env")

config_h_build([File('lib/config.h')], [File('lib/sc_config.h.in')], env)

SConscript(['libraptor/SConscript', 'lib/SConstruct', #'tools/SConstruct',
            'python2.6/SConstruct', #'tests/SConstruct',
            'applications/SConstruct'])

if 'debian' in COMMAND_LINE_TARGETS:
      SConscript("deb/SConscript")

# env.Package( NAME           = 'libaff4',
#              VERSION        = '0.1.rc1',
#              PACKAGEVERSION = 0,
#              PACKAGETYPE    = 'targz',
#              LICENSE        = 'gpl',
#              SUMMARY        = 'Advanced forensic fileformat V. 4',
#              DESCRIPTION    = 'AFF4 is a general purpose forensic file format',
#              X_RPM_GROUP    = 'Application/fu',
#              SOURCE_URL     = 'http://foo.org/hello-1.2.3.tar.gz'
#         )

