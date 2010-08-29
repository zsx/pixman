#!/usr/bin/env python
# encoding: utf-8
# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4

from waflibs import *
from waflib.Configure import conf, ConfigurationError
from waflibs.github.autoconf.defaults import INCLUDES_DEFAULT
import re

out = 'debug'
top = '.'

pixman_major = 0
pixman_minor = 19
pixman_micro = 3

VERSION= "%d.%d.%d" % (pixman_major, pixman_minor, pixman_micro)
APPNAME = 'pixman'

DECL_CODE = '''
int
main ()
{
#ifndef %s
#error Not declared
#endif

  return 0;
}
'''
OPENMP_CODE = '''
#include <stdio.h>

extern unsigned int lcg_seed;
#pragma omp threadprivate(lcg_seed)
unsigned int lcg_seed;

unsigned function(unsigned a, unsigned b)
{
	lcg_seed ^= b;
	return ((a + b) ^ a ) + lcg_seed;
}

int main(int argc, char **argv)
{
	int i;
	int n1 = 0, n2 = argc;
	unsigned checksum = 0;
	int verbose = argv != NULL;
	unsigned (*test_function)(unsigned, unsigned);
	test_function = function;
    #pragma omp parallel for reduction(+:checksum) default(none) \
					shared(n1, n2, test_function, verbose)
	for (i = n1; i < n2; i++)
    	{
		unsigned crc = test_function (i, 0);
		if (verbose)
			printf ("%d: %08X\n", i, crc);
		checksum += crc;
	}
	printf("%u\n", checksum);
	return 0;
}
'''
unsupported_options = re.compile(r'Command line warning \w* : ignoring unknown option')

@conf
def check_cflags(self, flag, code='', **kw):
    kw.update({'fragment': code + '\nint main(int c, char **v) { (void)c; (void)v; return 0; }',
              'msg': 'Checking whether compiler supports ' + flag,
              'compiler': 'c',
              'msvc_warn': unsupported_options,
              'cflags': flag,
              'ccflags': flag})
    self.check_compile_warn(**kw)

def options(opt):
        cfg = opt.parser.get_option_group('--prefix')
	bld = opt.parser.get_option_group('-p')

	cfg.add_option('--host', action='store', default=None, dest='host', help='cross-compile to build programs to run on HOST')
	cfg.add_option('--disable-mmx', action='store_false', dest='mmx', default=None, help='disable MMX fast paths')
        opt.tool_options("compiler_c")
        opt.tool_options("perl")

def configure(cfg):
	platinfo = PlatInfo()
	cfg.start_msg('Checking build system type:')
	cfg.end_msg(platinfo.fullname())

	cfg.start_msg('Checking host system type:')
	cfg.env.host = cfg.options.host
	if not cfg.env.host:
		cfg.env.host = platinfo
	else:
		cfg.env.host = PlatInfo.from_name(cfg.env.host)
	cfg.end_msg(cfg.env.host.fullname())

	cfg.check_tool('compiler_c')
	try:
		cfg.check_cc(fragment='int main(){return 0;}', execute=True, msg='Checking whether cross-compiling', okmsg='no', errmsg='yes')
		cfg.env.cross_compile=False
	except:
		cfg.env.cross_compile=True
        cfg.check_tool('compiler_c')
        cfg.check_cc(function_name='getisax', mandatory=False)
        cfg.check_endian()
        cfg.check_inline()
        env = cfg.env.derive()
        pixman_werror = None
        for flags in ('-Werror', '-errwarn'):
			try:
				cfg.check_cflags(flags)
				pixman_werror = flags
				break
			except:
				continue
        try:
			cfg.check_cc(fragment=INCLUDES_DEFAULT + DECL_CODE % '__amd64', msg='Checking whether __amd64 is defined') 
			AMD64_ABI = True
        except:
            try:
                cfg.check_cc(fragment=INCLUDES_DEFAULT + DECL_CODE % '_WIN64', msg='Checking whether _WIN64 is defined') 
                AMD64_ABI = True
            except:
                AMD64_ABI = False

        if cfg.env.CC_NAME == 'suncc':
			# Default CFLAGS to -O -g rather than just the -g from AC_PROG_CC
			# if we're using Sun Studio and neither the user nor a config.site
			# has set CFLAGS.
			if cfg.env.CFLAGS == ['-g']:
				cfg.env.CFLAGS = ['-O', '-g']
			# Sun Studio doesn't have an -xarch=mmx flag, so we have to use sse
			# but if we're building 64-bit, mmx & sse support is on by default and
			# -xarch=sse throws an error instead
			if not (getattr(self.env, MMX_CFLAGS, None) or AMD64_ABI):
				self.env.MMX_CFLAGS = ['-xarch=see']

        cfg.check_sizeof('long')

        cfg.check_cflags('-Wall', mandatory=False)
        cfg.check_cflags('-fno-strict-aliasing', mandatory=False)

        cfg.check_tool('perl')
        if not cfg.check_perl_version():
			self.fatal('Perl is required to build ' + APPNAME)
        try:
			cfg.check_openmp_cflags()
			env = cfg.env.derive()
			env.append_value('CCFLAGS', getattr(cfg.env, 'OPENMP_CFLAGS', []))
			cfg.check_cc(fragment=OPENMP_CODE, msg='Checking openMP support', env=env)
			cfg.define('USE_OPENMP')
        except ConfigurationError:
			pass
        
        try:
			cfg.check_cflags('-fvisibility=hidden', 
'''#if defined(__GNUC__) && (__GNUC__ >= 4)
#else\n error Need GCC 4.0 for visibility\n
#endif\n''')
        except ConfigurationError:
			cfg.check_cflags('-xldscope=hidden', '''#if defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
#else
error Need Sun Studio 8 for visibility
#endif''', mandatory=False)


        if not getattr(cfg.env, 'MMX_CFLAGS', None):
                cfg.env.MMX_CFLAGS = ['-mmmx', '-Winline']

	cfg.write_config_header('config.h')
    #print ("env = %s" % cfg.env)
    #print ("options = ", cfg.options)


def build(bld):
        pass
