import sys, pdb
import os
import SconsUtils.utils as utils
import distutils.sysconfig

from SconsUtils.utils import error, warn, add_option, generate_help, config_h_build

## Options
SetOption('implicit_cache', 1)

args = dict(CPPPATH='#include/', tools=['default', 'packaging'],
            package_version = '0.1',
            ENV = {'PATH' : os.environ['PATH'],
                   'TERM' : os.environ['TERM'],
                   'HOME' : os.environ['HOME']}
            )

## Add optional command line variables
vars = Variables()

vars.AddVariables(
   BoolVariable('V', 'Set to 1 for verbose build', False),
   EnumVariable('flavor', 'Choose build flavor', 'debug',
                allowed_values = ('release', 'debug'),
                ignorecase = 2,
                ),
   ('CC', 'The c compiler', 'gcc'),
   )

args['variables'] = vars

if ARGUMENTS.get('V') == "1":
   args['CFLAGS']='-Wall -g -O0'
else:
   utils.install_colors(args)

add_option(args, 'prefix',
           type='string',
           nargs=1,
           action='store',
           metavar='DIR',
           help='installation prefix')

add_option(args, 'disable_ewf', action='store_true', default=False,
              help = 'Disable compilation of libewf support')

add_option(args, 'disable_curl', action='store_true', default=False,
              help = 'Disable http support')

env = Environment(**args)

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
#    '-fomit-frame-pointer',
#    '-flto',
    ]

flavour = ARGUMENTS.get('flavor')
if (flavour == 'debug'):
   compileFlags.extend(['-ggdb', '-O0', '-Wall'])
   cppDefines.append('_DEBUG') #enables LOGGING
elif (flavour == 'release'):
    compileFlags.append('-O3')
    compileFlags.append('-fomit-frame-pointer')

# add the warnings to the compile flags
compileFlags += [ ('-W' + warning) for warning in warnings ]

env['CCFLAGS'] = compileFlags
env['CPPDEFINES'] = cppDefines

conf = Configure(env)

## Check for different things
if not env.GetOption('clean'):
   ## Mandatory dependencies
   if not conf.CheckLibWithHeader('z', 'zlib.h','c'):
      error( 'You must install zlib-dev to build libaff4!')

   if conf.CheckLibWithHeader('', 'uuid/uuid.h', 'c', call='uuid_generate();'):
      ## uuid is built in as in osX
      pass
   elif not conf.CheckLibWithHeader('uuid', 'uuid/uuid.h','c') or \
          not conf.CheckFunc('uuid_generate'):
      error('You must install uuid-dev to build libaff4!')

   if not conf.CheckLib('pthread') or \
          not conf.CheckFunc('pthread_create'):
      error('You must install pthread-dev to build libaff4!')

   ## Make sure the openssl installation is ok
   if not conf.CheckLib('ssl'):
      error('You must have openssl header libraries. This is often packaged as libssl-dev')

   for header in Split('openssl/aes.h openssl/bio.h openssl/evp.h openssl/hmac.h openssl/md5.h openssl/rand.h openssl/rsa.h openssl/sha.h openssl/pem.h'):
      if not conf.CheckHeader(header):
         error("Openssl installation seems to be missing header %s" % header)

   for func in Split('MD5 SHA1 AES_encrypt RAND_pseudo_bytes EVP_read_pw_string'):
      if not conf.CheckFunc(func):
         error("Openssl installation seems to be missing function %s" % func)

   if not conf.CheckLibWithHeader('raptor', 'raptor.h','c') or \
          not conf.CheckFunc('raptor_init'):
      error("You must have libraptor-dev installed")

   ## Optional dependencies
   if env["disable_ewf"]:
      warn("EWF Support disabled as requested by user")
   elif not conf.CheckLib('ewf'):
      env['disable_ewf'] = True

   if env["disable_curl"]:
      warn("HTTP Support disabled as requested by user")
   elif not conf.CheckLib('curl'):
      env['disable_curl'] = True


env = conf.Finish()

generate_help(vars, env)

Export("env")

## This environment is for compiling python extension modules
pythonenv = env.Clone()
pythonenv.Append(CPPFLAGS= ['-I%s' % distutils.sysconfig.get_python_inc()],
                 LDFLAGS = ['-L%s' % distutils.sysconfig.get_python_lib(),
                            distutils.sysconfig.get_config_var('LOCALMODLIBS')],
                 )
Export('pythonenv')

env.AlwaysBuild(env.Command('include/config.h', 'include/sc_config.h.in', config_h_build))

SConscript('lib/SConstruct')
SConscript('tools/SConstruct')
SConscript('python2.6/SConstruct')

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

